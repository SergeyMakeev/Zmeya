# Zmeya performance audit

This document summarizes performance characteristics and pitfalls found in the serialization / write path (primarily `Zmeya/Zmeya.h`), especially patterns that scale poorly compared to what callers might assume.

---

## 1. Incremental HashMap (snapshot + full rebuild)

**What happens:** Each `hashmap_insert`, `hashmap_erase`, and `hashmap_clear` walks the current `zm::HashMap`, copies logical contents into an `std::unordered_map`, applies the change, then calls `impl_assign_hashmap` (full rebuild of buckets and item slab in the arena).

**Complexity:** Theta(map size) per call. N incremental calls on a map that grows to size N yields Theta(N^2) total work.

**Extra cost (string keys):** The string-key path allocates a new `std::string` for every key when filling the temporary map (`std::string(it.first.c_str())` per entry per call). That adds heavy allocation and copying on top of the Theta(n) rebuild.

**Mitigation today:** Prefer bulk `assign` / `operator=` from `std::unordered_map` (or one-shot `w.root()->map = model`) for large maps. Reserve incremental APIs for small tables or tests.

**Code reference:** `hashmap_insert` / `hashmap_erase` / `hashmap_clear` in `Zmeya/Zmeya.h` (incremental path behind `ZMEYA_ENABLE_SERIALIZE_SUPPORT`).

---

## 2. Incremental HashSet (same pattern as HashMap)

**What happens:** `hashset_insert`, `hashset_erase`, and `hashset_clear` snapshot the `zm::HashSet` into an `std::unordered_set`, mutate, then `impl_assign_hashset`.

**Complexity:** Same as incremental HashMap: Theta(n) per operation, Theta(N^2) for N inserts into a growing set.

**Mitigation today:** Same as HashMap: bulk assign for large N.

---

## 3. Incremental String append

**What happens:** `string_append_cstr` / `String::append` allocate a new blob, `strlen` the existing C string, `memcpy` the entire old prefix, then append the suffix. Old bytes are marked dead until `finalize` compacts.

**Complexity:** Building total length L with many small appends copies the prefix repeatedly: Theta(L^2) character movement in the worst case (naive string growth). Repeated `strlen` on the growing prefix adds extra linear scans per append.

**Mitigation today:** Prefer a single `assign` from `std::string` (or build in `std::string` then assign once) for large strings.

---

## 4. Finalize: registry pruning on every dead range

**What happens:** `note_dead_range` calls `prune_roffset_registry_overlapping_dead`, which scans the entire `roffset_slot_targets_` map and may erase entries.

**Complexity:** Theta(R) per dead-range note, where R is the registry size. Many small mutations (strings, arrays, hashes) can approach Theta(R * D) work before `finalize`, where D is the number of dead-range notifications.

---

## 5. Finalize: compaction remap of registry

**What happens:** `compact_arena_and_remap_registry` builds a list of live `pieces`, then remaps every `(slot, target)` in `roffset_slot_targets_`. Each remap uses a linear scan over `pieces`.

**Complexity:** Theta(R * P) for that remapping phase, where P is the number of disjoint live segments after merging dead intervals. With many gaps, P can grow; with a huge registry, this can dominate `finalize`.

**Possible improvement:** Keep `pieces` ordered by `old_lo` and use binary search (or a single linear co-walk with sorted slot offsets) instead of scanning all pieces per goffset.

---

## 6. Patch pass (expected linear)

**What happens:** `patch_roffset_slots_from_registry` walks all registered slots and writes relative offsets.

**Complexity:** Theta(R). This is expected and usually not the bottleneck compared to sections 4-5 under extreme churn.

---

## 7. Array incremental behavior

### array_push_back

**What happens:** Capacity grows by doubling (similar to `std::vector`); each growth copies the old slab and marks it dead until compaction.

**Complexity:** Amortized O(1) per push for slab-memcpy-safe `T`. Not asymptotically mistaken, but arena can grow larger than a minimal one-shot layout (see AGENTS.md).

### array_erase_at

**What happens:** `memmove` over the tail.

**Complexity:** O(n) per erase for array size n. Erasing repeatedly from index 0 yields O(n^2) total.

### array_resize_fill (growth)

**What happens:** Grows by repeated `array_push_back`.

**Complexity:** Amortized linear in final size for trivial `T` (same doubling strategy).

---

## 8. Bulk impl_assign_hashmap / impl_assign_hashset

**What happens:** Full rebuild from `std::unordered_*`: two passes over `from`, hash computed twice per element (count pass + fill pass).

**Complexity:** Theta(n) for n elements. Appropriate for a wholesale rebuild; constant-factor duplication only, not mistaken quadratic growth.

---

## 9. assign_pointer

**What happens:** Updates self-relative offset to target; registers the slot.

**Complexity:** O(1). No issue.

---

## 10. Read path (deserialize / mmap)

**What happens:** `HashMap::find` / `HashSet::contains` scan the bucket chain (dense bucket ranges over a flat item array).

**Complexity:** O(chain length). Builder uses `numBuckets = 2 * n` so average chains stay short; worst case is standard hashing clustering.

---

## 11. Tests and expectations

Tests that loop `hashmap_insert` / `hashset_insert` many times inherit the incremental Theta(n) per call cost model. Example: `Coverage_P1_LargeNIncrementalHashMapVsGolden` documents O(N^2) behavior and uses smaller N in `_DEBUG` than in Release so the suite stays interactive.

Scaling such tests without adjusting N or switching to bulk assign will look like hangs in Debug.

---

## 12. Default write_blob arena reserve

**What happens:** The blob buffer starts with a modest default reserve (see `kDefaultWriteBlobArenaReserveBytes` in `Zmeya.h`).

**Effect:** Influences how often `std::vector<char>` reallocates during a session. Throughput / constant factor, not wrong asymptotics for output size.

---

## Proposed direction: HashMap / HashSet incremental performance

**Problem:** Today incremental hash operations decode the on-wire layout into STL containers and rebuild the dense bucket + item layout every time. That is simple and correct but Theta(n) per mutation.

**Suggested approach:** During `write_blob`, maintain hash tables with **open addressing** (no chaining lists that require stable pointers into separate slabs for every probe step in the way nested arrays do today for "bucket ranges"). Accept **no pointer stability** for entries inside this mutable table: probes are indices into a single flat array of slots (key/value or tombstone markers).

**Sizing:** Keep the open-address table roughly **2-4x** the number of live elements (load factor well below 1) to bound expected probes and simplify insert/delete; trade memory during the build for predictable O(1) amortized insert/erase.

**Finalize:** At `finalize()`, **emit the compact on-wire representation** (current dense buckets + contiguous items layout, or another read-optimized layout you standardize on) in one linear pass over the mutable table, then patch `roffset` targets as today.

**Trade-offs:**

- Implementation complexity: relocation, tombstones, rehash on growth, and keeping the roffset registry consistent with any moved slot fields (especially if keys or values contain `Pointer`, `String`, or nested containers) must be designed carefully.
- The incremental table is a **writer-only** structure; the mmap/read path can keep the current compact format if you want zero change for readers.
- Document that **raw pointers into the open-address arena are invalid** across insert/rehash (similar to existing guidance for vector reallocation on the blob buffer).

This matches the idea of fixing incremental hash performance without pretending each `hashmap_insert` is cheap at large n under the current snapshot-and-rebuild design.

---

## Summary table

| Area | Severity | Issue |
|------|----------|--------|
| Incremental HashMap / HashSet | High | Snapshot + full `impl_assign_*` each call: Theta(n) per op, Theta(N^2) batch; string keys worse. |
| Incremental String append | High (many appends) | Full prefix copy each append: Theta(L^2) to length L. |
| Registry prune per dead range | Medium under churn | Full scan of `roffset_slot_targets_` per `note_dead_range`. |
| Compaction remap | Medium at scale | Theta(R * P) remap via linear scan over `pieces` per entry. |
| array_erase_at from front | Medium if misused | Repeated front erase: Theta(n^2). |
| Bulk hash assign | Low (constant) | Two passes / double hash per element. |
| assign_pointer / read-side find | None expected | O(1) / usual hash lookup. |

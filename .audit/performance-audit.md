# Zmeya performance audit

**Last synced to implementation:** commit `62f1bd9e5d925379a66355f9871af8b971271787` (*remove audit results*, 2026-05-11).

## 1. Executive summary

- **Read path:** `HashMap::find` / `HashSet::contains` walk one bucket chain; average O(1) under good hashing; worst-case chain length is bounded in code but can approach Theta(n). Measured hot lookups on this machine are single-digit nanoseconds per op at n=4096 (see section 9).
- **Write path:** `zm::write_blob` runs the user lambda under TLS (`ScopedBuilder`), then `finalize_move_out` compacts dead ranges if any, pads to alignment, and patches every registered `roffset_t` slot. Patch cost is Theta(|registry|). Arena growth uses `std::vector<char>`; raw pointers into the arena invalidate across realloc (documented in `AGENTS.md` and `ZmeyaBlobWriter.inc`).
- **Incremental containers:** `HashMap` / `HashSet` use chained buckets with prepend; rehash when `live_count_ >= bucket_count` (load factor up to 1.0). Each rehash allocates a new bucket slab, walks all live nodes into a scratch `std::vector<uint32_t>`, and re-links; old bucket array is `note_dead_range` and compacted at finalize. Amortized behavior is typical of dynamic hashing; pathological keys keep worst-case chains.
- **Bulk vs incremental:** For int32 keys at n=4096, bulk `assign` from `std::unordered_map` is several times faster per completed blob than incremental `hashmap_insert` in `ZmeyaBench` (numbers in section 9). `hashmap_reserve_nodes` / `hashset_reserve_nodes` reduce repeated slab growth but do not remove per-insert chain work.
- **Strings / roffset pressure:** `BM_FinalizeManyStringRoffsets` and repeated string assign stress many relative offsets; cost grows with element count and registry size (patch pass is linear in registry).

## 2. Scope and hot paths

- **In scope:** Header-only library under `Zmeya/` (deserialize always; serialize paths behind `ZMEYA_ENABLE_SERIALIZE_SUPPORT`), builder fragments (`ZmeyaBuilder*.inc`), chained hash implementation (`ZmeyaHashMap.h`, `ZmeyaHashSet.h`, `ZmeyaBuilderHashChain.inc`), arena compaction (`ZmeyaBuilderBaseArena.inc`), registry patch (`ZmeyaBuilderBaseRegistry.inc`), benchmarks `ZmeyaBench.cpp`.
- **Excluded unless needed:** `extern/`, generated build trees.
- **Hot paths:** `detail::write_blob_with_initial_buffer_bytes` (lambda + `finalize_move_out`), `BuilderBase::finalize_in_place` / `compact_arena_and_remap_registry`, `patch_roffset_slots_from_registry`, `hashmap_insert` / `hashset_insert` / rehash, read-side `find` / `contains`, `const_iterator` traversal.

## 3. Implementation status (optional)

No separate in-flight refactor is tracked in this report; the audited tree matches commit above.

## 4. Findings

### 4.1 Overall health

Bulk assignment from STL containers uses snapshot-style builder logic tuned for one-shot layout. Incremental APIs match the same wire model but pay per-operation hashing, chain probes, occasional Theta(live_count) rehashes with auxiliary `std::vector<uint32_t>`, and extra `register_roffset_slot` traffic. That split is expected: incremental work trades convenience for constant-factor overhead versus a prepared `unordered_*` bulk load.

Finalize always walks the full `roffset_slot_targets_` map to write self-relative deltas; large graphs of pointers and strings grow registry size linearly. Arena compaction runs only when `dead_ranges_` is non-empty (typical after rehash leaves old bucket arrays as dead ranges); it sorts merges dead intervals, may prune registry entries overlapping dead bytes, memcpy-surviving spans into a new vector, and rebuilds several side `unordered_map` keyed by goffset (registry and builder bookkeeping maps).

### 4.2 Issue table

| Issue | Location | Status |
|-------|----------|--------|
| Finalize patch is Theta(registry entries); no early exit | `ZmeyaBuilderBaseRegistry.inc` `patch_roffset_slots_from_registry` | Unchanged / by design |
| Arena compaction cost scales with live bytes copied + registry remap maps | `ZmeyaBuilderBaseArena.inc` `compact_arena_and_remap_registry` | Unchanged / by design |
| Registry pruning vs merged dead ranges | `ZmeyaBuilderBaseArena.inc` `goffset_in_any_merged_dead_range` used by `prune_roffset_registry_overlapping_merged_dead` | Mitigated: point-in-union is O(log |merged|) per slot (merged intervals sorted, disjoint); prune pass remains Theta(|registry|) |
| Rehash allocates scratch `std::vector<uint32_t>` and scans all buckets | `ZmeyaBuilderHashChain.inc` `hashmap_rehash_impl` / `hashset_rehash_impl` | Unchanged / by design |
| Effective max load factor 1.0 before rehash; worst-case cluster chains | `ZmeyaBuilderHashChain.inc` (insert path), `ZmeyaHashMap.h` `findImpl` | Unchanged / by design; document hash quality |
| `const_iterator::seek_first` scans buckets until first non-empty | `ZmeyaHashMap.h`, `ZmeyaHashSet.h` | Mitigated: `empty()` fast-path skips bucket scan when `live_count_ == 0` |
| Raw pointers stale after arena growth | `ZmeyaBlobWriter.inc`, `ZmeyaBuilderHashChain.inc` header comment | Unchanged / by design (API contract) |
| Incremental vs bulk throughput gap for same n | `ZmeyaBench.cpp` (measured) | Mitigated partially by `*_reserve_nodes`; bulk still faster at tested n |

### 4.3 Hotspot details

**Layout and hashing:** Chained model stores `Bucket` array (head indices) and dense `Node` array with `uint32_t` next links. Inserts prepend to a bucket chain. Rehash grows bucket count to `newB = max(B * 2, 8)` when live count reaches bucket count (`ZmeyaBuilderHashChain.inc`).

**Rehash work:** Collect live node indices by walking every old bucket chain (Theta(old buckets + nodes touched)), allocate new bucket slab in arena, relink nodes, `note_dead_range` on old bucket array. Memory traffic is linear in live nodes for that event.

**Compaction:** Merges `dead_ranges_`, prunes registry entries pointing into dead bytes, copies surviving segments with `memcpy` into `newData`, builds `pieces` list, remaps every registry key/target via `upper_bound` over pieces (log #pieces per entry), replaces `roffset_slot_targets_` and optional phys/hint maps, `swap`s arena. After finalize, `dead_ranges_` must be empty (asserted in `finalize_in_place`).

**Read lookup:** `findImpl` modulo bucket count, walks chain with step cap `nodes.size() + 1` to detect corruption or pathological loops (`ZmeyaHashMap.h`).

**TLS write session:** `write_blob_with_initial_buffer_bytes` constructs `Builder`, installs `ScopedBuilder`, invokes functor, returns `finalize_move_out` which moves `std::vector<char>` out without an extra full-buffer copy of the finalized bytes (`ZmeyaBlobWriter.inc`).

### 4.4 Costs you cannot fix in one line

- Self-relative patching requires a pass over every recorded slot; skipping it would break the format.
- Compaction is required to reclaim arena space from superseded slabs (old hash bucket arrays) while keeping goffsets consistent; avoiding compaction would inflate blobs when many rehashes occur.
- Pointer stability across growth is fundamentally incompatible with a single growing `vector<char>` bump arena unless every client refreshes offsets; the library chooses explicit rebind guidance instead.

## 5. Open work and design options

1. **Benchmarks:** Add targeted benches for heavy `dead_ranges_` / compaction, iterator scans on sparse maps, and erase-heavy workloads (low risk).
2. **API usage:** Prefer bulk `assign` when the full STL map is already available; use `hashmap_reserve_nodes` for incremental to cut slab reallocations (already in benches; moderate win at n=4096 on this machine).
3. **Format / compatibility:** Changing load factor, bucket count policy, or chain encoding would be a wire-format decision; not proposed here.

## 6. Top practical fixes

1. **Workload choice:** Use bulk assign from `std::unordered_map` / `unordered_set` when data is already in memory; reserve incremental node pools when building incrementally.
2. **Minimize registry churn:** Fewer distinct pointer-relative fields and smaller string fan-out reduce finalize patch cost (architectural, not a micro-patch).
3. **After growth:** Re-derive pointers from `BlobWriter::root()`, `goffset_t`, or `builder_base()` per `AGENTS.md` whenever the arena might have grown.
4. **Measurement:** When tuning, run `ZmeyaBench` in Release on the target CPU; extend benches listed in section 9 for your dominant pattern (erase, mmap cold start, huge registry).

## 7. File references

| Topic | Files |
|-------|-------|
| `write_blob` / finalize entry | `Zmeya/ZmeyaBlobWriter.inc` |
| Builder arena, compaction, registry prune | `Zmeya/ZmeyaBuilderBase.inc`, `Zmeya/ZmeyaBuilderBaseArena.inc` |
| Roffset patch pass | `Zmeya/ZmeyaBuilderBaseRegistry.inc`, `Zmeya/ZmeyaBuilderBaseFinalize.inc` |
| Incremental hash insert/rehash/erase | `Zmeya/ZmeyaBuilderHashChain.inc` |
| Read-side map / find / iterator | `Zmeya/ZmeyaHashMap.h`, `Zmeya/ZmeyaHashSet.h` |
| Benchmark harness | `ZmeyaBench.cpp`, `run_perf_tests.cmd`, `AGENTS.md` |

## 8. Conclusion

Zmeya's costs are well-aligned with a bump arena plus explicit relative-pointer patching: write throughput is dominated by incremental hash work and occasional rehash/compaction, while read-side lookups stay cache-friendly for typical distributions. Use the measured Release splits (section 9) as a baseline on the machine under test; validate on your target with the same harness before treating constant factors as universal.

## 9. Benchmark coverage and gaps

### 9.1 What was measured (this session)

Commands used (from repo root, existing `build-bench\Release\ZmeyaBench.exe`): Google Benchmark with `--benchmark_min_time=0.05s` and `--benchmark_repetitions=1` unless noted. Hardware from benchmark banner: 24 logical CPUs reported at 3187 MHz. Values below are **Wall Time** and **CPU Time** as printed by Google Benchmark (two columns); `items_per_second` is the library counter from each benchmark.

**Subset at n=4096 (int keys / int array) or stated otherwise:**

| Benchmark | Wall Time | CPU Time | items_per_second |
|-----------|-----------|----------|------------------|
| BM_HashMapInt32_BulkAssign/4096 | 40646 ns | 12455 ns | 328.86M/s |
| BM_HashMapInt32_IncrementalInsert/4096 | 224166 ns | 93376 ns | 43.8654M/s |
| BM_HashMapInt32_IncrementalInsert_Reserved/4096 | 192373 ns | 83724 ns | 48.9226M/s |
| BM_HashMapInt32_IncrementalEraseReinsert/4096 | 287276 ns | 97656 ns | 41.943M/s |
| BM_HashSetInt32_BulkAssign/4096 | 33627 ns | 17188 ns | 238.313M/s |
| BM_HashSetInt32_IncrementalInsert/4096 | 182298 ns | 108994 ns | 37.5802M/s |
| BM_HashSetInt32_IncrementalInsert_Reserved/4096 | 163481 ns | 77853 ns | 52.6123M/s |
| BM_ArrayInt32_BulkAssign/4096 | 10696 ns | 3488 ns | 1.17441G/s |
| BM_ArrayInt32_PushBack/4096 | 42071 ns | 21797 ns | 187.92M/s |
| BM_HashMapInt32_FindHit/4096 | 4.82 ns | 2.50 ns | 399.624M/s |
| BM_HashMapInt32_FindMiss/4096 | 7.91 ns | 4.19 ns | 238.933M/s |
| BM_HashSetInt32_ContainsHit/4096 | 4.68 ns | 2.18 ns | 458.752M/s |
| BM_StringRepeatedAssignFinalize/128 | 29002 ns | 9375 ns | 13.6533M/s |
| BM_FinalizeManyStringRoffsets/512 | 73202 ns | 34877 ns | 14.6801M/s |

**String keys at n=512:**

| Benchmark | Wall Time | CPU Time | items_per_second |
|-----------|-----------|----------|------------------|
| BM_HashMapStringInt32_BulkAssign/512 | 85047 ns | 54497 ns | 9.39505M/s |
| BM_HashMapStringInt32_IncrementalInsert/512 | 233590 ns | 93376 ns | 5.48318M/s |
| BM_HashMapStringInt32_IncrementalInsert_Reserved/512 | 165948 ns | 85638 ns | 5.97867M/s |
| BM_HashMapStringInt32_FindHit/512 | 10.5 ns | 4.38 ns | 228.571M/s |

**Windows mmap read (same find pattern, file-backed view):**

| Benchmark | Wall Time | CPU Time | items_per_second |
|-----------|-----------|----------|------------------|
| BM_Win32Mmap_HashMapInt32FindHit/4096 | 4.77 ns | 1.17 ns | 856.337M/s |

Interpretation guardrails: wall vs CPU time differs under system noise; `items_per_second` uses the benchmark harness accounting rules. Do not compare mmap vs in-memory `items_per_second` as a strict apples-to-apples ratio without reading Google Benchmark counter definitions for each case. Absolute nanoseconds are machine-specific.

### 9.2 Gaps and proposed tests

**Landed after audit:** `BM_HashMapInt32_EraseHeavy` and `BM_HashMapInt32_IterateAll` in `ZmeyaBench.cpp` (same range grid as other int32 map benches). Registry prune and iterator `empty()` fast-path were implemented in library code (see issue table).

Not run this session: full default range sweeps, Linux mmap, allocator profiling (heap counts), or `ZMEYA_BUILDER_PARANOID` builds.

**Proposed additions to `ZmeyaBench.cpp` (concrete harness work):**

1. **Name:** `BM_HashMapInt32_IncrementalInsertOnlyRehashes`  
   **Shape:** Pre-size with `hashmap_reserve_nodes` then insert n keys that force many rehash boundary crossings vs random insert order.  
   **Compare:** Wall time vs existing `BM_HashMapInt32_IncrementalInsert_Reserved` at same n.  
   **Pass criteria:** Stable `items_per_second` trend; optional manual profile to confirm time in `hashmap_rehash_impl`.

2. **Name:** `BM_HashMapInt32_EraseHeavy` (**landed**)  
   **Shape:** Insert n, erase half (even keys), insert `need` fresh keys in one `write_blob`.  
   **Compare:** Against `BM_HashMapInt32_IncrementalEraseReinsert`.  
   **Pass criteria:** Detect regressions in erase plus refill behavior.

3. **Name:** `BM_CompactionManyRehashesSmallMap`  
   **Shape:** Repeatedly grow a small map until k rehashes occur in one session (tune n and insert pattern) producing multiple `note_dead_range` segments; measure full `write_blob` time.  
   **Compare:** Against same final element count built with bulk assign (no intermediate dead ranges).  
   **Pass criteria:** Quantify compaction + remap overhead when `dead_ranges_.size()` is large.

4. **Name:** `BM_HashMapInt32_IterateAll` (**landed**; audit draft name `BM_HashMapIterateAllEntries`)  
   **Shape:** After bulk build of n entries, full forward iteration each timed iteration.  
   **Compare:** n and bucket count scaling.  
   **Pass criteria:** Linear scan cost visibility for sparse vs dense bucket tables.

5. **Name:** `BM_FinalizeRegistryPressure` (optional)  
   **Shape:** Root with many `zm::Pointer` or nested structures registering many roffset slots without huge payload (or mirror `BM_FinalizeManyStringRoffsets` with wider fan-out).  
   **Pass criteria:** Isolate `patch_roffset_slots_from_registry` dominance via profiler-guided iteration counts.

**How to reproduce locally (Windows, Visual Studio generator):** run `run_perf_tests.cmd` from repo root (configures `build-bench`, builds Release `ZmeyaBench`). For stable numbers, use Release, close background load, and consider `--benchmark_repetitions=3` for noisy cases.

# Zmeya performance audit

**Last synced to implementation:** post-change (STL snapshot path removed; node slab grow in `ZmeyaBuilderHashChainNodesGrow.inc`, 2026-05-12).

## 1. Executive summary

- **Read path:** Deserialization uses chained hash tables (`HashMap` / `HashSet`). `find` walks one bucket chain; expected average **Theta(1)** per lookup under usual hashing assumptions; worst-case chain length **Theta(n)**. Full iteration is **Theta(n)** over live nodes; `begin()` may scan **Theta(B)** buckets before the first element (sparse tables).
- **Write path:** `zm::write_blob` runs a builder session then **finalize** (compact dead ranges if any, align, patch self-relative slots). Incremental **`hashmap_*` / `hashset_*`** always mutate the chained arena layout (**`hashmap_chain_*` / `hashset_chain_*`**). Dense **`nodes[]`** growth uses **`BuilderBase::hash_chain_nodes_array_grow_append_default_*`** in **`ZmeyaBuilderHashChainNodesGrow.inc`**: slab reallocate, per-slot **`placementCtor`** plus field copy (strings via **`assign_string_std`** from a stack **`std::string`** copy of the old key), then **`~Node()`** on the old slab slot (no **`std::unordered_*`** snapshot of the whole table).
- **Biggest cliffs:** (1) **Rehash** on chain tables when `live_count_ >= bucket_count` (**Theta(n)** work, amortized across growth). (2) **Arena compaction** when dead ranges exist (copies live ranges, remaps registry). (3) **Finalize patch pass** linear in registered roffset slots.

## 2. Scope and hot paths

- **Library:** `Zmeya/` headers and `.inc` fragments (excluding `extern/`).
- **Hot paths:** `detail::write_blob_with_initial_buffer_bytes` -> user lambda -> `BuilderBase::finalize_move_out` -> `finalize_in_place` -> optional `compact_arena_and_remap_registry`, padding, `patch_roffset_slots_from_registry`.
- **Incremental APIs:** `BlobWriter::hashmap_*`, `hashset_*`, `array_*`, `string_*`; member mutators under TLS (`AGENTS.md`).
- **Tests:** `ZmeyaTest*.cpp` exercise mmap (`ZmeyaTest10.cpp`) and incremental behavior (`ZmeyaTestIncremental.cpp`, `ZmeyaTestCoverage.cpp`); Release vs Debug allocation counts differ (`AGENTS.md`, `ZmeyaTest04`).

## 3. Implementation status (optional)

Incremental hash APIs no longer branch on **`zm_hashmap_chain_incremental_ok`** / **`zm_hashset_chain_incremental_ok`** for STL snapshots; those traits are **`std::true_type`** for documentation compatibility.

## 4. Findings

### 4.0 STL snapshot rebuilds (resolved)

**Previous behavior:** When **`zm_hashmap_chain_incremental_ok`** / **`zm_hashset_chain_incremental_ok`** was false, **`ZmeyaSerializeApiIncremental.inc`** imported the live table into **`std::unordered_*`**, edited, then **`impl_assign_*`**.

**Current behavior:** **`hashmap_insert` / `hashmap_erase` / `hashmap_clear` / `hashset_insert` / `hashset_erase` / `hashset_clear`** always call **`BuilderBase::hashmap_chain_*` / `hashset_chain_*`**. **`hashmap_try_in_place_update`** still short-circuit-updates existing keys without growing **`nodes[]`**.

Bulk **`assign` from `std::unordered_map` / `std::unordered_set`** remains the intended path when the caller already holds STL containers.

### 4.1 Overall health

Finalize cost scales with **how many self-relative words were registered**, not only logical payload size.

### 4.2 Issue table

| Issue | Location | Status |
|-------|----------|--------|
| Full-table STL snapshot on incremental hash APIs | was `ZmeyaSerializeApiIncremental.inc` | **Removed**; chain + **`ZmeyaBuilderHashChainNodesGrow.inc`** slab grow |
| **`zm_hashmap_chain_incremental_ok` / `zm_hashset_chain_incremental_ok` tied to `zm_array_push_back_ok`** | `ZmeyaHashMap.h`, `ZmeyaHashSet.h` | Traits are **`std::true_type`**; **`zm_array_push_back_ok`** still gates public **`Array::push_back`** only |
| Rehash doubles buckets at load 1.0 | `ZmeyaBuilderHashChain.inc` | By design; amortized |
| Arena compaction copies live runs, rebuilds `roffset_slot_targets_` map | `ZmeyaBuilderBaseArena.inc` | By design when dead ranges exist |
| Finalize patches every registry slot | `ZmeyaBuilderBaseRegistry.inc` | Unchanged; Theta(registry size) |
| Raw pointers into arena invalidated across growth | `ZmeyaBuilderHashChain.inc` header comment; `AGENTS.md` | Documented |
| `write_blob` returns moved buffer | `ZmeyaBlobWriter.inc` | Mitigated (move, not extra vector copy) |

### 4.3 Hotspot details

**Layout / hashing (read):** `HashMap::findImpl` / `HashSet::containsImpl` hash to a bucket index then walk `next` pointers. A step counter caps work at `nodes.size() + 1`.

**Incremental hash (write):** Chain insert probes bucket chain, may call **`hashmap_rehash_impl`** / **`hashset_rehash_impl`** when `live_count_ >= B` (**Theta(n)** for that event). New nodes append via **`hash_chain_nodes_array_grow_append_default_*`** (see **`ZmeyaBuilderHashChainNodesGrow.inc`**).

**Arena:** Default reserve **`kDefaultWriteBlobArenaReserveBytes`** = 64 KiB (`ZmeyaSerializeFoundation.h`). Growth uses `std::vector<char>`; realloc invalidates raw pointers until refreshed.

**Compaction:** If **`dead_ranges_`** non-empty, merged intervals drive memcpy of retained pieces; **`roffset_slot_targets_`** remapped through **`std::unordered_map`** rebuild (`ZmeyaBuilderBaseArena.inc`).

### 4.4 Costs you cannot fix in one line

- **Finalize registry:** Every stored relative pointer slot must be patched; cost is **linear in registered slots** (`patch_roffset_slots_from_registry`).
- **Wire format:** Chained representation is kept through finalize (`ZmeyaBuilderHashChain.inc` comment); read side stays pointer-chasing friendly for mmap.

## 5. Open work and design options

1. **Benchmarks:** **`ZmeyaBench`** (CMake **`ZMEYA_BUILD_BENCHMARKS=ON`**, **`run_perf_tests.cmd`**) contrasts bulk vs incremental and string-key paths; use **`--benchmark_repetitions`** for stable numbers.
2. **API usage:** Bulk **`assign`** when you already have a full STL map/set to write once.

## 6. Top practical fixes

1. **Avoid stale pointers:** Recompute from `goffset_t` / `w.root()` after any growth (`AGENTS.md`).
2. **Expect larger arenas:** Bump allocator leaves abandoned slabs; compare logical content after finalize, not minimal byte length (`AGENTS.md`).
3. **Measure before micro-optimizing:** Rehash and finalize dominate at large **n**; run **`ZmeyaBench`** for hypotheses.

## 7. File references

| Topic | Files |
|-------|-------|
| write_blob / finalize | `ZmeyaBlobWriter.inc`, `ZmeyaBuilderBaseFinalize.inc` |
| Arena compact / dead ranges | `ZmeyaBuilderBaseArena.inc` |
| Registry patch | `ZmeyaBuilderBaseRegistry.inc` |
| Incremental hash API | `ZmeyaSerializeApiIncremental.inc` |
| Serialize API include order (assign then node grow) | `ZmeyaSerializeApi.inc` |
| Hash node slab growth (chain tables) | `ZmeyaBuilderHashChainNodesGrow.inc` (**`hash_chain_nodes_array_grow_append_default_*`**) |
| Chain hash implementation | `ZmeyaBuilderHashChain.inc` |
| Read-side find / iterate | `ZmeyaHashMap.h`, `ZmeyaHashSet.h` |
| array_push_back gate | `ZmeyaArray.h` |
| Default arena reserve | `ZmeyaSerializeFoundation.h` |
| Repo workflow / flags | `AGENTS.md` |

## 8. Conclusion

Incremental hash mutation no longer uses per-operation **full-table `std::unordered_*` snapshots**; it always uses the chain APIs with explicit node-slab growth. Remaining dominant costs are **rehash**, optional **arena compaction**, and **finalize registry patching**. **`ZmeyaBench`** supplies measured comparisons for bulk vs incremental workloads.

## 9. Benchmark coverage and gaps

**Measured in this document:** None (code inspection and complexity notes).

**Already in `ZmeyaBench.cpp`:** HashMap int/int and String/int bulk vs incremental insert; HashSet int bulk vs incremental; Array int bulk vs push_back; find-hit fixtures; repeated string assign / finalize stress.

**Proposed additions (for implementers):**

1. **`hashmap_erase` / `hashset_erase`:** bulk rebuild vs one-by-one erase for large **n**; string-key erase curves.
2. **Finalize / registry isolation:** synthetic root with many pointer-relative fields or nested containers; measure wall time with **`--benchmark_min_time=1s`** and **`--benchmark_repetitions=5`**.
3. **Stability:** document or add a wrapper script that passes **`--benchmark_repetitions`** and **`--benchmark_report_aggregates_only=true`** for CI or audit attachments.
4. **Larger n:** extend ranges toward 10^5 / 10^6 where runtime allows, for bulk vs incremental curves.

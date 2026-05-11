# Zmeya performance audit

**Last synced to implementation:** commit `3bc67f40a5be3cb61fbd21eae854935d7db0a45f` (*+audit skills*, 2026-05-11).

## 1. Executive summary

- **Read path:** Deserialization uses chained hash tables (`HashMap` / `HashSet`). `find` walks one bucket chain; expected average **Theta(1)** per lookup under usual hashing assumptions; worst-case chain length **Theta(n)**. Full iteration is **Theta(n)** over live nodes; `begin()` may scan **Theta(B)** buckets before the first element (sparse tables).
- **Write path:** `zm::write_blob` runs a builder session then **finalize** (compact dead ranges if any, align, patch self-relative slots). Incremental hash edits use **amortized Theta(1)** chain work only when `zm_hashmap_chain_incremental_ok` / `zm_hashset_chain_incremental_ok` is true (requires **`zm_array_push_back_ok`** key/value types: trivially copyable slab elements; excludes `zm::String`, `zm::Pointer`, nested `zm::Array`). See **section 4.0** for the exact **STL resnapshot** behavior when that gate is false (it is real code today, not hypothetical).
- **Biggest cliffs:** (1) Non-incremental hash mutation types. (2) **Rehash** on chain tables when `live_count_ >= bucket_count` (**Theta(n)** work, amortized across growth). (3) **Arena compaction** when dead ranges exist (copies live ranges, remaps registry). (4) **Finalize patch pass** linear in registered roffset slots.
- **Still Theta(n) per op on slow incremental hash path (4.0):** `hashmap_insert` / `hashmap_erase` import the whole map into STL then `impl_assign_hashmap` when the chain gate is false. Non-`String` key erase uses an explicit per-element loop plus `impl_assign_hashmap` when excluding a key.

## 2. Scope and hot paths

- **Library:** `Zmeya/` headers and `.inc` fragments (excluding `extern/`).
- **Hot paths:** `detail::write_blob_with_initial_buffer_bytes` -> user lambda -> `BuilderBase::finalize_move_out` -> `finalize_in_place` -> optional `compact_arena_and_remap_registry`, padding, `patch_roffset_slots_from_registry`.
- **Incremental APIs:** `BlobWriter::hashmap_*`, `hashset_*`, `array_*`, `string_*`; member mutators under TLS (`AGENTS.md`).
- **Tests:** `ZmeyaTest*.cpp` exercise mmap (`ZmeyaTest10.cpp`) and incremental behavior (`ZmeyaTestIncremental.cpp`); Release vs Debug allocation counts differ (`AGENTS.md`, `ZmeyaTest04`).

## 3. Implementation status (optional)

No separate refactor branch audited; report reflects current `main` at the commit above.

## 4. Findings

### 4.0 STL snapshot rebuilds (policy vs code)

**Product stance (what you want):** Incremental **`hashmap_*` / `hashset_*`** APIs should **not** rebuild the live table by copying it through a temporary **`std::unordered_map` / `std::unordered_set`** and then **`impl_assign_*`** (full rewrite of that container in the arena).

**What the implementation does today (facts):** The file **`ZmeyaSerializeApiIncremental.inc`** implements **two** behaviors:

1. **Chain path (no STL snapshot of the whole table):** When **`zm_hashmap_chain_incremental_ok<K,V>`** or **`zm_hashset_chain_incremental_ok<Key>`** is true, **`hashmap_insert` / `hashmap_erase` / `hashmap_clear` / `hashset_insert` / `hashset_erase` / `hashset_clear`** call **`BuilderBase::hashmap_chain_*` / `hashset_chain_*`** only. Those paths mutate the chained arena layout directly.

2. **Slow path (explicit STL snapshot + full rebuild):** When the trait above is **false**, the same APIs **do** import the current **`zm::`** map/set into a temporary **`std::unordered_*`**, apply the logical edit there, then call **`impl_assign_hashmap` / `impl_assign_hashset`**. Concretely: **`hashmap_stl_import_from_zm`**, **`hashmap_stl_fill_from_zm_excluding`**, **`hashset_stl_fill_from_zm`**, **`hashset_stl_fill_from_zm_skipping`**, plus empty-map **`impl_assign_*`** on **`clear`**. Comment in source: *"O(hm.size()) snapshot + full rebuild"* on **`hashmap_insert`** slow branch.

**Exception (still no full-table snapshot):** **`hashmap_try_in_place_update`** runs first on **`hashmap_insert`**. If the key **already exists**, it can **`find`** and overwrite the value **in place** and return (e.g. **`HashMap<zm::String,V>`** with **`std::string`** key, or **`FK == K`**). **New keys** on the slow path still go through the STL snapshot sequence above.

So: **never STL-resnapshot** is **not** true of the current tree for gated-out key/value types; the audit treats that as a **documented implementation choice** tied to **`zm_array_push_back_ok`**, not as absent code.

### 4.1 Overall health

Bulk **`assign` from `std::unordered_map` / `std::unordered_set`** is the intended batch path when you already have STL data. It is **not** the same mechanism as the slow incremental branch, but both end up rewriting the **`zm::`** container from an **`std::unordered_*`**. For hot incremental **`String`** keys (or other non-incremental_ok types), prefer **bulk `assign`** until a true chain incremental path exists for those types. Finalize cost scales with **how many self-relative words were registered**, not only logical payload size.

### 4.2 Issue table

| Issue | Location | Status |
|-------|----------|--------|
| Incremental hash only when `zm_array_push_back_ok<Key>` (and Value for maps) | `ZmeyaHashMap.h` `zm_hashmap_chain_incremental_ok`, `ZmeyaHashSet.h` `zm_hashset_chain_incremental_ok`, `ZmeyaArray.h` `zm_array_push_back_ok` | By design / documented |
| Full-table STL snapshot on incremental hash APIs when chain gate false | `ZmeyaSerializeApiIncremental.inc` `hashmap_insert`, `hashmap_erase`, `hashmap_clear`, `hashset_insert`, `hashset_erase`, `hashset_clear` | **Present in code**; conflicts with a strict *no resnapshot* product rule unless types are incremental_ok or only in-place updates |
| Insert without chain path: `hashmap_stl_import_from_zm` + `impl_assign_hashmap` | same file `hashmap_insert` after `hashmap_try_in_place_update` miss | Open for gated-out types; mitigated when incremental_ok |
| Erase without chain path: `hashmap_stl_fill_from_zm_excluding` or manual loop + `impl_assign_hashmap` | same file `hashmap_erase` | Open for gated-out types |
| String-key snapshot allocates `std::string` per entry | `hashmap_stl_import_from_zm`, `hashmap_stl_fill_from_zm_excluding`, `hashset_stl_*` | Unchanged; Major constant-factor cost |
| Rehash doubles buckets at load 1.0 | `ZmeyaBuilderHashChain.inc` `hashmap_rehash_impl` / `hashset_rehash_impl` | By design; amortized |
| Arena compaction copies live runs, rebuilds `roffset_slot_targets_` map | `ZmeyaBuilderBaseArena.inc` `compact_arena_and_remap_registry` | By design when dead ranges exist |
| Finalize patches every registry slot | `ZmeyaBuilderBaseRegistry.inc` `patch_roffset_slots_from_registry` | Unchanged; Theta(registry size) |
| Raw pointers into arena invalidated across growth | `ZmeyaBuilderHashChain.inc` header comment; `AGENTS.md` | Documented |
| `write_blob` returns moved buffer | `ZmeyaBlobWriter.inc` | Mitigated (move, not extra vector copy) |

### 4.3 Hotspot details

**Layout / hashing (read):** `HashMap::findImpl` hashes to a bucket index then walks `next` pointers (`ZmeyaHashMap.h`). Cycle guard caps steps at `nodes.size() + 1`.

**Incremental hash (write):** Chain insert probes bucket chain, may call **`hashmap_rehash_impl`** when `live_count_ >= B` (**Theta(n)** for that event). Node storage reuses `free_head_` or appends via **`array_push_back`** on `nodes` (requires slab-safe node fields).

**Arena:** Default reserve **`kDefaultWriteBlobArenaReserveBytes`** = 64 KiB (`ZmeyaSerializeFoundation.h`). Growth uses `std::vector<char>`; realloc invalidates raw pointers until refreshed.

**Compaction:** If **`dead_ranges_`** non-empty, merged intervals drive memcpy of retained pieces; **`roffset_slot_targets_`** remapped through **`std::unordered_map`** rebuild (`ZmeyaBuilderBaseArena.inc`).

### 4.4 Costs you cannot fix in one line

- **Finalize registry:** Every stored relative pointer slot must be patched; cost is **linear in registered slots** (`patch_roffset_slots_from_registry`).
- **Wire format:** Chained representation is kept through finalize (`ZmeyaBuilderHashChain.inc` comment); read side stays pointer-chasing friendly for mmap.
- **Type gate:** Relaxing `zm_array_push_back_ok` would break slab memcpy relocation assumptions unless the format or relocation rules change (**compatibility** risk).

## 5. Open work and design options

1. **Eliminate STL resnapshot slow path (if required by product rules):** Extend chain incremental support to **`zm::String`** (and other types excluded by **`zm_array_push_back_ok`**) without breaking slab relocation rules, or implement another strategy that never copies the full live map through **`std::unordered_*`** on single-key APIs. Until then, the snapshot branch in **`ZmeyaSerializeApiIncremental.inc`** remains the implemented fallback for gated-out types.
2. **Benchmarks:** **`ZmeyaBench`** (CMake **`ZMEYA_BUILD_BENCHMARKS=ON`**, **`run_perf_tests.cmd`**) already contrasts bulk vs incremental and string-key paths; use **`--benchmark_repetitions`** for stable numbers.
3. **API usage:** Prefer bulk **`assign`** for large batches when key/value types are not incremental_ok (`AGENTS.md` already guides this).
4. **Format / ABI:** Any change to incremental eligibility or hash layout is a **compatibility** decision; coordinate with consumers.

## 6. Top practical fixes

1. **Choose types deliberately:** Use trivially copyable keys/values for hot incremental hash paths; use bulk assign for `zm::String`-key maps when doing many updates.
2. **Avoid stale pointers:** Recompute from `goffset_t` / `w.root()` after any growth (`AGENTS.md`).
3. **Expect larger arenas:** Bump allocator leaves abandoned slabs; compare logical content after finalize, not minimal byte length (`AGENTS.md`).
4. **Measure before micro-optimizing:** Snapshot paths dominated by **`std::string` construction** for String keys need profiling to justify changes.

## 7. File references

| Topic | Files |
|-------|-------|
| write_blob / finalize | `ZmeyaBlobWriter.inc`, `ZmeyaBuilderBaseFinalize.inc` |
| Arena compact / dead ranges | `ZmeyaBuilderBaseArena.inc` |
| Registry patch | `ZmeyaBuilderBaseRegistry.inc` |
| Incremental hash API / snapshot fallback | `ZmeyaSerializeApiIncremental.inc` |
| Chain hash implementation | `ZmeyaBuilderHashChain.inc` |
| Read-side find / iterate | `ZmeyaHashMap.h`, `ZmeyaHashSet.h` |
| array_push_back gate | `ZmeyaArray.h` |
| Default arena reserve | `ZmeyaSerializeFoundation.h` |
| Repo workflow / flags | `AGENTS.md` |

## 8. Conclusion

Zmeya splits clearly between **fast incremental chain mutation** for slab-safe element types and **Theta(n) full-table STL snapshot + `impl_assign_*` rebuilds** on the slow incremental branch for other combinations (see **4.0**). That is **not** the same as *no resnapshot ever* unless you only use incremental_ok types or rely on in-place key updates where applicable. Finalize work is **linear in relative-pointer registry size** plus optional compaction. **`ZmeyaBench`** supplies measured comparisons for the main cases listed in section 5.

## 9. Benchmark coverage and gaps

**Measured in this document:** None. This audit was written from **code inspection** plus complexity arguments; no benchmark table is pasted above. Treat hot-path ordering as **hypothesis** until you run **`run_perf_tests.cmd`** (or **`ZmeyaBench`** with **`ZMEYA_BUILD_BENCHMARKS=ON`**) and attach or cite output.

**Already in `ZmeyaBench.cpp`:** HashMap int/int and String/int bulk vs incremental insert; HashSet int bulk vs incremental; Array int bulk vs push_back; find-hit fixtures; repeated string assign / finalize stress.

**Proposed additions (for implementers):**

1. **`hashmap_erase` / `hashset_erase`:** bulk rebuild vs one-by-one erase for incremental_ok types; String-key erase on slow path (expect Theta(n) per call).
2. **Finalize / registry isolation:** synthetic root with many pointer-relative fields or nested containers; measure wall time with **`--benchmark_min_time=1s`** and **`--benchmark_repetitions=5`**.
3. **Stability:** document or add a wrapper script that passes **`--benchmark_repetitions`** and **`--benchmark_report_aggregates_only=true`** for CI or audit attachments.
4. **Larger n:** extend ranges toward 10^5 / 10^6 where runtime allows, for bulk vs incremental curves.

# Zmeya performance audit

**Last synced to implementation:** branch `reimplement_builder`, subject *Address perf audit: string slab copy, docs, benches, audit refresh* (2026-05-11). Use `git rev-parse HEAD` for the exact hash of your checkout.

## 1. Executive summary

- **Read path:** `HashMap::find` walks a singly linked bucket chain; average case O(1) under good hashing, worst case Theta(n) per lookup if chains grow. Measured hot lookups on finalized blobs are cheap for `int32_t` keys at n=4096; `zm::String` keys cost more due to key comparison.
- **Write path:** `write_blob` uses a bump arena in `detail::BuilderBase::data` (`std::vector<char, BufferAllocator<...>>`). Growth can reallocate; raw pointers into the arena are invalid across growth unless refreshed (documented in `AGENTS.md`).
- **Incremental hash containers:** `hashmap_insert` / `hashset_insert` trigger **rehash** when `live_count_ >= bucket_count` (effective max load factor **1.0**). Each rehash is Theta(n) over live nodes plus new bucket array allocation. `hashmap_reserve_nodes` / `hashset_reserve_nodes` reduces node-slab growth work but does not remove rehash steps when load hits 1.0.
- **Finalize:** `finalize_in_place` optionally compacts dead ranges (Theta(arena moves + registry remaps) when `dead_ranges_` is non-empty), pads to alignment, then **`patch_roffset_slots_from_registry`**, which is **Theta(R)** for R entries in `roffset_slot_targets_`.
- **String-key incremental growth:** When the node slab reallocates, `zm::String` fields are re-materialized with `assign_string_cstr` from the old slot `c_str()` (no `std::string` staging on that path), which still allocates new blob char storage and runs `strlen` on the source.
- **Bulk vs incremental:** For `HashMap<int32_t,int32_t>` at n=4096 on the machine used for this audit, **bulk assign** measured substantially lower CPU time per full `write_blob` than **incremental insert** (numbers in section 9).

## 2. Scope and hot paths

- **In scope:** Header-only library under `Zmeya/` (split `.inc` implementation), `ZmeyaBench.cpp`, `AGENTS.md` guidance. Excluded unless noted: `extern/`, generated build trees.
- **Hot paths:** `zm::write_blob` session (TLS `BuilderBase`), `hashmap_insert` / `hashset_insert` / slab growth, `array_push_back` (trait-gated element types), `assign` from STL containers, `finalize_in_place` / `patch_roffset_slots_from_registry`, read-side `HashMap::findImpl` and iteration.
- **Tests / I/O:** `ZmeyaTest10.cpp` uses Windows `MapViewOfFile` for read-only mapping of a written blob; other platforms may read into a buffer instead. That is an integration concern, not a microbench of the core layout code.

## 3. Implementation status (optional)

- Audit syncs to HEAD above (*improve performance*). No separate before/after refactor table in this run.

## 4. Findings

### 4.1 Overall health

- **Bulk serialization** from STL (`operator=` / `assign` from `std::unordered_map` etc.) tends to batch work and avoids per-insert probe chains in the benchmark harness; it remains the throughput winner where you already have the STL structure.
- **Incremental APIs** pay for per-operation registry updates, chain walks, occasional **Theta(n)** rehashes, and (for string keys) slab migration that re-encodes each live `zm::String` into the arena when the node slab doubles.
- **Average vs worst case:** Open chaining: documented in `HashMap` / `HashSet` headers (average O(1) probes, worst Theta(n), step cap in `findImpl` / `containsImpl`). Adversarial clustering remains a **format-level** concern if you ever lowered load factor.

### 4.2 Issue table

| Issue | Location | Status |
|-------|----------|--------|
| Finalize patch is linear in number of registered self-relative slots | `detail::BuilderBase::patch_roffset_slots_from_registry` in `Zmeya/ZmeyaBuilderBaseRegistry.inc` | Unchanged / by design |
| Arena compaction scans registry and rebuilds `std::unordered_map` side tables when `dead_ranges_` non-empty | `detail::BuilderBase::compact_arena_and_remap_registry` in `Zmeya/ZmeyaBuilderBaseArena.inc` | Unchanged / by design; rare if few dead slabs |
| Rehash on insert when `live_count >= bucket_count` (load factor up to 1.0) | `Zmeya/ZmeyaBuilderHashChain.inc` (`hashmap_rehash_impl`, `hashset_rehash_impl`) | Unchanged / by design |
| Read `find` walks chain; worst Theta(n) | `zm::HashMap::findImpl` in `Zmeya/ZmeyaHashMap.h` | Mitigated (documented in header; algorithm unchanged) |
| `zm::String` node slab grow copies keys through `std::string` | `detail::BuilderBase::hash_chain_nodes_array_grow_append_*`, `hashmap_chain_reserve_node_pool` / `hashset_chain_reserve_node_pool` in `Zmeya/ZmeyaBuilderHashChainNodesGrow.inc`, `Zmeya/ZmeyaBuilderHashChain.inc` | Mitigated (`assign_string_cstr` from old `c_str()`; avoids `std::string` staging) |
| Arena `vector` reallocation invalidates raw pointers | `AGENTS.md`, `ZmeyaSerializeFoundation.h` | Unchanged / by design |
| No benchmark coverage for erase-heavy maps, compaction stress, or mmap latency | `ZmeyaBench.cpp` | Mitigated (new cases: erase/reinsert, find miss, set contains, string reserved insert, many string roffsets, Win32 mmap find) |

### 4.3 Hotspot details

**Registry and finalize**

- `roffset_slot_targets_` is a `std::unordered_map<goffset_t, goffset_t>` (`Zmeya/ZmeyaBuilderBase.inc`). Every patchable self-relative word registers a slot; finalize walks all entries once (`Zmeya/ZmeyaBuilderBaseRegistry.inc`).

**Compaction**

- If `dead_ranges_` is empty, compaction is skipped (`Zmeya/ZmeyaBuilderBaseArena.inc`).
- Otherwise: merge intervals Theta(D log D), prune registry entries overlapping dead ranges (nested loops over merged dead intervals per registry entry), memcpy live spans into a new arena buffer, binary-search remap for each registry key/target, rebuild several auxiliary `unordered_map`s. This is **not** on the steady-state path if you avoid patterns that leave many dead slabs before finalize.

**Hash chains (write)**

- Initial bucket count 8; rehash doubles bucket count when load would exceed 1.0 (`Zmeya/ZmeyaBuilderHashChain.inc` comments and `live_count_ >= B` checks).
- Rehash collects live node indices by scanning all buckets (Theta(B + n)), allocates a new bucket array, rewires heads in the new table.

**Hash chains (read)**

- `findImpl` modulo-hash then walks `next` pointers with a step cap tied to `nodes.size()` (`Zmeya/ZmeyaHashMap.h`).

**Arrays**

- `array_push_back` is restricted to slab-memcpy-safe element types (`detail::zm_array_push_back_ok` in `Zmeya.h` per `AGENTS.md`); nested blobs use `assign` instead.

### 4.4 Costs you cannot fix in one line

- **Self-relative patching:** Any format with relative pointers needs a final pass or equivalent; R grows with pointer-rich graphs.
- **Bump allocator + abandoned slabs:** Documented in `AGENTS.md`; logical size can be smaller than a minimal one-shot layout for the same content after many growth cycles.
- **Load factor 1.0:** Improves memory for bucket tables but increases expected chain length versus lower alpha; changing it is a **wire-layout / compatibility** trade if bucket counts change for the same insertion order.

## 5. Open work and design options

1. **API / usage:** Prefer `hashmap_reserve_nodes` when final `n` is known to reduce slab reallocations (modest win in measured int32 run; see section 9).
2. **Format / algorithm (high risk):** Changing load factor, bucket growth policy, or hash layout affects blobs on disk; treat as compatibility project.
3. **Synthetic collision harness** (section 9.3): still optional for stress-testing worst-case chains.

## 6. Top practical fixes

1. For large incremental maps/sets with known final size, call **`hashmap_reserve_nodes` / `hashset_reserve_nodes`** before inserts to cut node-slab reallocations (`AGENTS.md`, `ZmeyaBench.cpp` reserved variants).
2. When the STL model already exists, **`assign` from `std::unordered_map` / `std::unordered_set`** remains the fastest way to build equivalent maps in the current harness (section 9).
3. After any arena growth, **re-derive pointers** from `BlobWriter` / `goffset_t` (`AGENTS.md`); treating this as mandatory avoids correctness bugs that look like intermittent perf cliffs.
4. If compaction shows up in profiles (many `note_dead_range` uses), reduce churn that abandons large slabs before finalize, or extend benchmarks to prove the need for algorithmic changes.

## 7. File references

| Topic | Files |
|-------|-------|
| Builder arena, registry maps, dead ranges | `Zmeya/ZmeyaBuilderBase.inc`, `Zmeya/ZmeyaBuilderBaseArena.inc` |
| Finalize phases | `Zmeya/ZmeyaBuilderBaseFinalize.inc` |
| Patch loop | `Zmeya/ZmeyaBuilderBaseRegistry.inc` |
| Hash insert / rehash / erase | `Zmeya/ZmeyaBuilderHashChain.inc` |
| Node slab growth (string key copy path) | `Zmeya/ZmeyaBuilderHashChainNodesGrow.inc` |
| Read `find` / `contains` | `Zmeya/ZmeyaHashMap.h`, `Zmeya/ZmeyaHashSet.h` |
| Microbenchmarks | `ZmeyaBench.cpp` |
| Build/run bench | `run_perf_tests.cmd`, `AGENTS.md` |

## 8. Conclusion

Measured data on one Windows Release build shows **bulk assign dominates incremental insert** for the tested int32 map at n=4096, and **read-side `find`** for int32 keys is orders of magnitude cheaper per call than a full `write_blob`. String-key incremental insert is much slower than string-key bulk assign at n=512 in the same harness, consistent with extra per-insert work and slab growth behavior. Follow-up work added benchmarks for erase/reinsert, find miss, set `contains`, many string roffsets (finalize stress), string reserved incremental insert, and Win32 mmap read + `find`. Remaining optional work: synthetic collision harness and dedicated compaction byte-ratio microbench if profiles warrant it.

## 9. Benchmark coverage and gaps

### 9.1 What was measured (this session)

Environment: **Windows**, **Release** `ZmeyaBench.exe` from `build-bench\Release\`, Google Benchmark aggregates (**mean** of **5** repetitions), **`--benchmark_min_time=0.1s`**. CPU model reported as **24 x ~3187 MHz**. Times are **environment-specific**; use as relative signal, not portable absolutes.

**HashMap int32_t, n=4096 (mean CPU time per iteration of the benchmark loop; `items_per_second` is Google Benchmark user counter where present):**

| Benchmark | CPU mean | items_per_second (mean) |
|-----------|----------|-------------------------|
| `BM_HashMapInt32_BulkAssign/4096` | ~19.5 us | ~355 M/s |
| `BM_HashMapInt32_IncrementalInsert/4096` | ~106 us | ~39 M/s |
| `BM_HashMapInt32_IncrementalInsert_Reserved/4096` | ~91.9 us | ~50 M/s |
| `BM_HashSetInt32_IncrementalInsert/4096` | ~96.9 us | ~45.4 M/s |
| `BM_ArrayInt32_PushBack/4096` | ~22.2 us | ~188 M/s |
| `BM_HashMapInt32_FindHit/4096` | ~2.55 ns per `find` | ~412 M/s |

**HashMap string key, n=512:**

| Benchmark | CPU mean | items_per_second (mean) |
|-----------|----------|-------------------------|
| `BM_HashMapStringInt32_BulkAssign/512` | ~53.7 us | ~9.76 M/s |
| `BM_HashMapStringInt32_IncrementalInsert/512` | ~150 us | ~3.96 M/s |
| `BM_HashMapStringInt32_FindHit/512` | ~6.91 ns per `find` | ~150 M/s |

**Added benchmarks (smoke on Release, single machine, not full sweep):** `BM_HashMapInt32_IncrementalEraseReinsert`, `BM_HashMapInt32_FindMiss`, `BM_HashSetInt32_ContainsHit`, `BM_HashMapStringInt32_IncrementalInsert_Reserved`, `BM_FinalizeManyStringRoffsets`, and (Windows only) `BM_Win32Mmap_HashMapInt32FindHit`.

**Not run this session:** full range sweeps for all benchmarks, `BM_StringRepeatedAssignFinalize`, allocation counters (`--benchmark_perf_counters` where supported), Linux/gcc numbers.

### 9.2 Inferred from code only (needs measurement if claimed in isolation)

- Cost of **`compact_arena_and_remap_registry`** vs number of dead ranges and registry size.
- Worst-case **long-chain** read latency under adversarial keys.
- **`finalize_move_out`** vs `finalize` returning `Span` when the caller copies bytes anyway.

### 9.3 Proposed tests still optional

1. **Compaction byte-ratio bench:** Many `hashmap_insert` / `String` overwrites vs one-shot assign; record final `blob.size()` ratio (complements `ZmeyaTest` coverage tests).
2. **Synthetic collision harness:** Keys forced into long chains (if a test-only hook exists); measure **`find`** latency -- label **synthetic** in reports.
3. **POSIX mmap read:** Mirror `BM_Win32Mmap_HashMapInt32FindHit` with `mmap` where available for cross-platform I/O comparison.

Reproduce commands (from repo root on Windows):

```text
run_perf_tests.cmd --benchmark_filter=... --benchmark_min_time=0.1s --benchmark_repetitions=5 --benchmark_report_aggregates_only=true
```

Use **Release** for timing; Debug asserts and extra epoch counters can dominate.

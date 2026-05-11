# Zmeya performance audit and new hash design proposal

This document records a performance review of the Zmeya header-only blob library (serialize + mmap read path) and the **Option C** direction: **index chaining** in the blob with **std::unordered_map-like** invalidation semantics.

**Last synced to implementation:** commit **`0d7cb68`** (*Option C chained hash tables and builder realloc safety*, 2026-05-11). Sections below distinguish **landed behavior** vs **still open** work.

---

## 1. Executive summary

- **Read path:** `HashMap` / `HashSet` now use **on-disk index chaining** (bucket `head` + dense `nodes[]` with `uint32_t next`, `ZMEYA_HASH_CHAIN_NIL`), not the older **flat `items` + per-bucket `[begin,end)` index ranges**. **Find** / **contains** walk a **linked index chain** per bucket; **iteration** scans buckets then chains (forward iterators). Locality is **chain-bound** rather than one contiguous `items` slab per table.
- **Write path (incremental hash):** For keys (and map values) that satisfy **`zm_hashset_chain_incremental_ok<Key>`** / **`zm_hashmap_chain_incremental_ok<K,V>`** (same idea as **`zm::array_push_back_ok`**: trivially copyable / slab-safe), **`hashmap_insert`**, **`hashset_insert`**, **`erase`**, and **`clear`** use **`BuilderBase::hashmap_*_chain_*`** in **`ZmeyaBuilderHashChain.inc`** — **amortized O(1)** per op under a fixed **rehash when `live_count >= bucket_count`** policy (effective **max load factor 1.0**), **8** initial buckets, **prepend** to chains, **free list** on erase. **No per-op `impl_assign_*` full rebuild** on that path.
- **Still Theta(n) per op:** **`zm::String`** keys (and any key/value type that fails the chain traits) continue to use **STL snapshot + `impl_assign_*`** from **`ZmeyaSerializeApi.inc`** (including the expensive **`std::string` per survivor** pattern on string-key **`hashmap_erase`** in the snapshot branch).
- **Unchanged cliffs:** **`string_append`** quadratic risk, **`write_blob`** return **full vector copy**, **`finalize`** compaction / registry costs — see audit tables.
- **Correctness / perf hygiene:** Builder code **re-binds** `HashMap` / `HashSet` pointers from **`hm_g` / `hs_g`** after **`alloc_aligned`** so references into **`data`** do not dangle across **`std::vector` realloc** (documented in **`ZmeyaBuilderHashChain.inc`**).

---

## 2. Implementation status (commit `0d7cb68`)

**Commit:** `0d7cb68c164d991d301a0dec1a23065c47f0c943`  
**Message:** Option C chained hash tables and builder realloc safety  

**Highlights (from commit message and tree):**

| Area | Change |
|------|--------|
| **Wire format** | Chained **`HashMap` / `HashSet`** through **`finalize`** (no flatten-to-flat-items pass). |
| **Incremental path** | **`ZmeyaBuilderHashChain.inc`**: insert/erase/clear/rehash for slab-safe keys; **`impl_assign_*`** refreshed for chained layout and **pointer refresh after growth**. |
| **Serialize API** | **`ZmeyaSerializeApi.inc`**: `if constexpr (zm_hash*_chain_incremental_ok)` dispatch to chain APIs; else legacy STL snapshot + **`impl_assign_*`**. |
| **Read API** | **`HashMap`**: iterator **`operator*`** returns **`std::pair<const Key&, const Value&>`** so **`zm::String`** keys are not shallow-copied during iteration. **`HashSet`**: **`const_iterator`** over nodes. |
| **Docs / safety** | Comments on **invalidation** (like **`std::unordered_*`**) during **`write_blob`**; **Option C** and **dangling `hm`/`hs`** notes in builder includes. |
| **Tests** | **`ZmeyaTestNewAPI.cpp`**, coverage / test tweaks for new behavior and logical compares. |
| **Build** | **`Zmeya/CMakeLists.txt`** (+1 line). |

**Files touched (stat):** `Zmeya/CMakeLists.txt`, `ZmeyaArray.h`, `ZmeyaBuilderAssignImpl.inc`, `ZmeyaBuilderBase.h`, `ZmeyaBuilderBase.inc`, **`ZmeyaBuilderHashChain.inc`** (new), `ZmeyaHashAdapters.h`, `ZmeyaHashMap.h`, `ZmeyaHashSet.h`, `ZmeyaSerializeApi.inc`, `ZmeyaString.h`, `ZmeyaTest08.cpp`, `ZmeyaTestCoverage.cpp`, `ZmeyaTestNewAPI.cpp`.

**Trait gates (incremental chain vs snapshot):**

- **`zm_hashset_chain_incremental_ok<Key>`** — `ZmeyaHashSet.h`: **`detail::zm_array_push_back_ok<Key>::value`**.
- **`zm_hashmap_chain_incremental_ok<K,V>`** — `ZmeyaHashMap.h`: **`zm_array_push_back_ok<K> && zm_array_push_back_ok<V>`**.

So **`HashSet<zm::String>`**, **`HashMap<zm::String, …>`**, maps with **`zm::String`** values, nested **`zm::Array`**, etc. **do not** get the fast incremental path until those types are supported without **`impl_assign`** rebuilds.

---

## 3. Scope and hot paths

Zmeya is a **header-only blob serializer**; hot paths are not game loops but:

- User code inside **`zm::write_blob`** (construction, incremental APIs).
- **`BuilderBase::finalize`** (compaction, registry remap, **`roffset`** patch pass).
- Read-side **`HashMap::find`**, **`HashSet::contains`**, **chain iteration**.

---

## 4. Performance audit: findings (updated)

### 4.1 Overall health

- **Bulk `assign` from STL** and **read-only mmap** remain **Theta(n)** one-time work; **`impl_assign_hash*`** now emits the **chained** layout (bucket slab + node slab, **`live_count_`**, **`free_head_`**).
- **Incremental hash:** **split verdict**
  - **Slab-safe key (and map value):** **amortized O(1)** via chain insert/erase/rehash; **rehash** collects node indices in a **`std::vector<uint32_t>`** (**Theta(n)** rare event, not per insert).
  - **`zm::String` / non-slab-safe:** still **Theta(n)** snapshot + **`impl_assign_*`** per call where that branch runs — **same critical risk as before** for string-heavy incremental workloads.
- **`hashmap_insert` update-in-place** for existing key (find hits) remains **O(chain length)** without full rebuild on both paths where **`find`** applies.
- **String incremental append**, **`write_blob`** copy, **`finalize`** costs — unchanged from prior audit (sections 4.4–4.5).

### 4.2 Issue table (severity after `0d7cb68`)

| Issue | Location | Status after commit |
|-------|----------|---------------------|
| Full hash rebuild **per incremental op** | `ZmeyaSerializeApi.inc` | **Mitigated** for **`zm_hash*_chain_incremental_ok`** types; **unchanged** for **`String`** keys / bad value types |
| String-key **`hashmap_erase`** allocs | Snapshot branch in `ZmeyaSerializeApi.inc` | **Still critical** on that branch |
| Chatty **`string_append`** | `ZmeyaBuilderBase.inc` | **Still major** |
| Mandatory return copy of blob | `ZmeyaBlobWriter.inc` | **Still major** |

### 4.3 Read-side `HashMap` / `HashSet` (current layout)

- **Layout:** **`Array<Bucket>`** with **`uint32_t head`**; **`Array<Node>`** with **`Key`**, (**`Value`** for map), **`uint32_t next`**. **`live_count_`**, **`free_head_`** on the container.
- **Find:** hash mod **`buckets.size()`**, walk **`head` -> next** chain — **O(1)** average, **O(n)** worst bucket.
- **Iteration:** **O(B + N)** bucket scan + chain steps; **not** a single dense **`items`** scan — can be **more cache misses** than old flat **`items`** layout for full-table walks.
- **`HashMap` iterator** exposes **references** (pair of **`const Key&`**, **`const Value&`**) to avoid copying **`zm::String`** keys by value during iteration.

### 4.4 `finalize` (unchanged big picture)

- Merge/sort dead ranges, copy live runs, prune registry (**Theta(R * D)** worst), remap registry (**Theta(R log P)**), linear patch (**Theta(R)**).
- Fewer **full hash rebuilds** during the session **reduce dead slab churn** from **`impl_assign_*`** for slab-safe incremental use — **secondary** benefit to **`finalize`** cost.

### 4.5 Rehash auxiliary allocation

- **`hashset_rehash_impl` / `hashmap_rehash_impl`** allocate **`std::vector<uint32_t> ord`** sized **`live_count`** to walk old chains in deterministic order — **Theta(n)** memory **per rehash**, not per insert. Rare compared to insert rate if load policy is sane.

---

## 5. Historical note: why snapshot existed

Earlier incremental APIs **reused** **`impl_assign_*`** by **STL snapshot + full rebuild** — **Theta(n)** per call by choice.

**Commit `0d7cb68`** adds a **second path**: **in-blob index chaining** for slab-safe types so **`std::unordered_map`**-style **amortized** behavior is achievable **without** rebuilding the entire table on every insert.

**String keys** still follow the snapshot path because **`zm::String`** is not **`array_push_back_ok`** for the node slab model; extending Option C to string keys would need a **different node representation** or **arena string** handling without **`impl_assign`** each time.

---

## 6. Design options (status)

| Option | Idea | Status (2026-05-11) |
|--------|------|---------------------|
| **A** | STL authoritative + one **`assign`** | Still valid workaround for **string-key** incremental churn |
| **B** | Chained mutable + **finalize flatten** to old flat **`items`** | **Not** implemented; wire format is **chained end-to-end** |
| **C** | Chained on-disk + incremental mutation | **Implemented** for **`zm_hash*_chain_incremental_ok`**; **partial** for **`String`** |

---

## 7. Option C specification (design + landed subset)

### 7.1 Goals (unchanged intent)

| Goal | Landed |
|------|--------|
| Amortized **O(1)** insert/erase/find (average) | **Yes** for slab-safe **K** (**V** for maps) |
| **std::unordered_*-like** invalidation docs | **Yes** (headers + builder includes) |
| **Mmap** single blob, no STL on read | **Yes** |
| **Index chaining** | **Yes**: **`ZMEYA_HASH_CHAIN_NIL`**, prepend, free list |

### 7.2 Policy (as implemented)

- **Initial buckets:** **8** (see **`ZmeyaBuilderHashChain.inc`**).
- **Rehash trigger:** **`live_count_ >= bucket_count`** (**effective max load factor 1.0** for chaining).
- **Growth:** **`new_bucket_count = max(old_bucket_count * 2, 8)`** (see **`hashset_chain_insert`** / **`hashmap_chain_insert`** in **`ZmeyaBuilderHashChain.inc`**).

### 7.3 Logical format (aligned with code)

Header fields include **`live_count_`**, **`free_head_`**, **`Array<Bucket> buckets`**, **`Array<Node> nodes`** (see **`ZmeyaHashMap.h`**, **`ZmeyaHashSet.h`**). **`Node`** stores **`key`** and **`value`** separately on the map so builders need not default-construct **`Pair<const K,V>`**.

### 7.4 Read path: complexity and locality

| Operation | Expected |
|-----------|----------|
| **find** / **contains** | **O(1)** average; **O(n)** worst chain |
| **iterate all** | **O(B + N)** |

**vs old flat `items` + ranges:** iteration may **lose** some sequential scan locality; measure if full-table scans matter.

### 7.5 Open work (not in `0d7cb68`)

- **Incremental `String` keys** (and **`zm::String`** values) without STL snapshot.
- **Optional finalize flatten** to a flatter read encoding if benchmarks demand it.
- **SoA** / **iteration list** optimizations from the original proposal.

---

## 8. Suggested implementation order (revised)

1. ~~Read chained layout + find + iterate~~ **Done**
2. ~~Builder incremental insert/erase/rehash for slab-safe types~~ **Done**
3. ~~**`impl_assign_*`** for chained wire format~~ **Done**
4. **String-key incremental chain** (or documented permanent STL batching).
5. **Benchmarks:** chained read scan vs old golden blobs (intentionally **not** backward compatible — compare **logical** equivalence only).
6. **`write_blob`** avoid full output copy (optional API).

---

## 9. Complexity reference table (before vs after `0d7cb68`)

| Area | Before `0d7cb68` | After `0d7cb68` (slab-safe K, V) | After (String K and/or non-slab V) |
|------|------------------|----------------------------------|-------------------------------------|
| **`hashmap_insert` (new key)** | Theta(n) | Amortized O(1) | Theta(n) snapshot |
| **`hashset_insert`** | Theta(n) | Amortized O(1) | Theta(n) snapshot |
| **`erase` / `clear`** | Theta(n) | Amortized O(1) erase; clear Theta(n) one pass | Theta(n); string erase alloc heavy |
| **Rehash** | N/A (rebuild) | Theta(n) time + **`ord`** alloc | Same as insert path |
| **Read `find`** | O(1) avg | O(1) avg (chain walk) | Same |
| **Read full scan** | Flat **`items`** | Bucket + chain hops | Same |

---

## 10. Top practical fixes (updated)

1. For **`zm::String`** keys or non-slab values: **batch in STL + one `assign`** — still the main fix until chain path supports them.
2. **`string_append`**: build **`std::string`** then **`assign`** for large payloads.
3. **Output copy:** consider API avoiding **`std::vector<char>`** copy from finalized span.
4. **Profile `finalize`** when mixing heavy incremental **non-hash** churn with large **`roffset`** registries.

---

## 11. File references (codebase)

| Topic | Files |
|-------|--------|
| Incremental dispatch (chain vs snapshot) | `Zmeya/ZmeyaSerializeApi.inc` |
| Chain insert/erase/rehash, realloc discipline | **`Zmeya/ZmeyaBuilderHashChain.inc`** |
| Bulk build / chained init in **`impl_assign_*`** | `Zmeya/ZmeyaBuilderAssignImpl.inc` |
| Arena, string append, **`finalize`** | `Zmeya/ZmeyaBuilderBase.inc` |
| Builder include order | `Zmeya/ZmeyaBuilderBase.h` (**includes** hash chain **.inc**) |
| Read **`HashMap` / `HashSet`**, traits | `Zmeya/ZmeyaHashMap.h`, `Zmeya/ZmeyaHashSet.h` |
| **`write_blob`** return copy | `Zmeya/ZmeyaBlobWriter.inc` |

---

## 12. Conclusion

**Commit `0d7cb68`** lands **Option C index chaining** as the **on-disk** representation and implements **true incremental** insert/erase/clear/rehash for **slab-safe** key/value types, fixing the **Theta(n) per incremental op** problem for that subset. **`zm::String`**-keyed (and similar) paths remain on the **legacy snapshot + `impl_assign_*`** strategy; **string-key erase** remains a **high allocator / time** risk until addressed.

The audit’s non-hash items (**`string_append`**, **output copy**, **`finalize`**) are unchanged. Next wins are **string-aware incremental** support, **benchmarks** on chained **iteration** vs old layouts, and optional **API** / **flatten** work driven by measured read cost.

---

*Document updated after commit `0d7cb68`; revise when string-key chain support or finalize flatten lands.*

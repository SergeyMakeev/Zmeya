# Zmeya performance audit and new hash design proposal

This document records a performance review of the Zmeya header-only blob library (serialize + mmap read path) and a design direction for **efficient incremental `HashMap` / `HashSet` mutation** using **chaining** with **std::unordered_map-like semantics**. Backward compatibility is **out of scope** for the design section until the format is frozen.

---

## 1. Executive summary

- **Read path** is generally sound: flat `items` plus bucket metadata gives good locality for typical loads.
- **Write path (incremental hash APIs)** is the main scalability cliff: each insert/erase resnapshots and calls `impl_assign_*`, which is **Theta(n) per call** and **Theta(n^2)** over many operations.
- The fix is **not** "pick open addressing vs chaining" in the abstract; the fix is to **stop rebuilding the whole table on every logical edit** (or to hold authoritative state in STL and encode once).
- This document ends with a **concrete Option C** proposal: **chained table in the blob** (index-based), **rehash policy**, and clear tradeoffs vs today's flat layout.

---

## 2. Scope and hot paths

Zmeya is a **header-only blob serializer**; hot paths are not game loops but:

- User code inside **`zm::write_blob`** (construction, incremental APIs).
- **`BuilderBase::finalize`** (compaction, registry remap, `roffset` patch pass).
- Read-side **`HashMap::find`**, **`HashSet::contains`**, iteration over `Array` / hash `items`.

---

## 3. Performance audit: findings

### 3.1 Overall health

- **Bulk `assign` from STL** and **read-only mmap** use are aligned with Theta(n) one-time work.
- **Incremental `hashmap_insert` / `hashset_insert` / `hashset_erase` / `hashmap_erase`** (with `ZMEYA_ENABLE_SERIALIZE_SUPPORT`) are the primary **algorithmic** risk: full snapshot + full rebuild per call (see `ZmeyaSerializeApi.inc`).
- **String incremental append** uses **`strlen` on the existing prefix** and **copies the full string** on each growth step; many small appends can approach **quadratic** total work in the string length and create **dead slab** churn until `finalize`.
- **`write_blob`** returns a **new `std::vector<char>`** by copying the finalized span: **Theta(blob_size)** extra time and **double** peak memory for the output.
- **`finalize`**: merge/sort dead ranges, copy live runs, **prune** `roffset_slot_targets_` against merged dead intervals (**Theta(R * D)** with R registry entries and D merged dead segments in the worst case), **remap** registry using **binary search** on live **pieces** (**Theta(R log P)**), then **linear patch** over R.

### 3.2 Critical issues (summary)

| Issue | Location | Why it matters |
|-------|----------|----------------|
| Full hash rebuild per incremental op | `ZmeyaSerializeApi.inc` | Theta(n) per op; m ops can be Theta(m * n) |
| String-key `hashmap_erase` materializes `std::string` for survivors | `hashmap_erase` String branch | Theta(n) heap allocs per erase in the worst case |
| Chatty `string_append` | `ZmeyaBuilderBase.inc` `string_append_cstr` | Repeated `strlen` + full prefix copy; quadratic risk |
| Mandatory return copy of blob | `ZmeyaBlobWriter.inc` | Full-buffer memcpy; 2x peak memory |

### 3.3 Read-side `HashMap` / `HashSet`

- **Layout:** bucket array + **index ranges** into a **flat `items` array** (contiguous storage per build). Find scans the **chain** for that bucket (index range) in **`items`**: average O(1) with good distribution; **worst case Theta(n)** if all keys land in one bucket.
- **Locality:** good for **scanning one bucket** and for **sequential layout of `items`**; not the same as **open addressing probing** in one array.

### 3.4 What `finalize` does (relevant to any new design)

- **Compaction** copies **live** byte runs into a new buffer; **remap** updates **`goffset_t`** keys/values in `roffset_slot_targets_` using **sorted** live **pieces** and `upper_bound` per entry.
- **Pointer/index-heavy mutable structures** must either **minimize** patchable edges, **defer** compaction, or run a **dedicated remap pass** over the structure.

---

## 4. Why the current incremental hash path resnapshots (design note)

The current implementation **chooses** simplicity:

- **`impl_assign_hashmap` / `impl_assign_hashset`** (`ZmeyaBuilderAssignImpl.inc`) already build a **complete**, **read-oriented** layout from a full STL snapshot (hash keys, bucket counts, prefix-sum placement into flat **`items`**).
- Incremental APIs **reuse that single entry point** by copying into **`std::unordered_map` / `std::unordered_set`**, mutating, and calling **`impl_assign_*`** again.

So **Theta(n) per call** is an **engineering tradeoff**, not a requirement of arenas or `roffset`. **std::unordered_map** does not rebuild on every insert; **Zmeya's incremental wrapper** effectively does, by always going through **full layout rebuild**.

**Compacting bump allocator + scattered chunks:** In principle the arena **could** hold a **true chained** or **open-addressed** table with **`Pointer` / `roffset` / indices**. The hard part is implementing **incremental** insert/erase/rehash **correctly** with **relocation and patching** rules — not the existence of the allocator.

---

## 5. Design options (complexity vs effort)

| Option | Idea | Incremental complexity | Wire format | Effort |
|--------|------|------------------------|-------------|--------|
| **A** | Authoritative **`std::unordered_*`** during `write_blob`; **one `assign`** at end | Amortized O(1) per STL op | Unchanged | Low |
| **B** | **Arena-scattered** mutable graph (**nodes + links**); **finalize** flattens to today's flat reader layout | Can be amortized O(1) if implemented | Can stay unchanged after finalize | High |
| **C** | **New on-disk** chained (or other) layout optimized for **incremental mutation + mmap read** | Amortized O(1) with policy | **New** | High |

This document focuses on **Option C** as requested: **best-effort design**, **chaining**, **semantically aligned with `std::unordered_map` / `unordered_set`**, **backward compatibility not required for now**.

---

## 6. Option C: chained hash in the blob (design proposal)

### 6.1 Goals

| Goal | Implication |
|------|-------------|
| **Amortized O(1)** insert / erase / find under normal load | True incremental structure; **rehash** only when load policy triggers |
| **Semantics close to `std::unordered_*`** | Buckets + linked entries, explicit **load factor** / **rehash** policy |
| **Mmap-friendly reads** | Single blob; **no STL** on read path; relative addressing only |
| **Arena compatibility** | Prefer **index chaining** or a controlled **remap** story to avoid registering every intratable edge |

### 6.2 Preferred internal representation: **index chaining** in a dense node pool

**Pure `roffset`-chained nodes** work but can explode **registry / patch** work on **compaction** (every `next` and bucket head).

**Recommended:** **append-only (or tombstoned) node pool** with **uint32_t** links:

- **`nodes[]`:** dense array of nodes (or **SoA**: separate arrays for `next`, keys, values if profiling warrants).
- **`next_idx`:** index into `nodes`, with a reserved value for **null** (e.g. `UINT32_MAX` or `0` — pick one and document).
- **`buckets[]`:** `uint32_t head_idx` per bucket (length **B**).

**Insert:** append a node at the end of `nodes` (or reuse from a **free list** after erase); splice into `buckets[h % B]` (policy: **front** vs **back** — document).

**Erase:** unlink; **tombstone** or **free-list** the index.

**Why indices help:** If **`nodes` and `buckets` live in slabs** that move as **single ranges** during compaction, **indices** into the **same slab** can remain valid after **one** base fixup; if compaction **splinters** the node pool, either **defer** compaction for that pool until finalize or run an **O(N)** index / pointer **fixup** pass (still better than **Theta(n)** work **per insert**).

### 6.3 Logical format sketch

```text
HashMap<K,V> header (in-root struct, conceptual):
  uint32_t bucket_count
  uint32_t size                 // live element count
  optional: capacity / free list head
  roffset_t buckets_g           // -> BucketHeader[B]
  roffset_t nodes_g             // -> node storage

BucketHeader:
  uint32_t head_idx             // reserved value = empty bucket

Node (conceptual):
  K key
  V value
  uint32_t next_idx
```

**HashSet\<K\>** is the same without `V`.

**Key/value layout:** Same constraints as today: trivial / relocatable rules, **`zm::String`** as separate blob payloads, etc.

### 6.4 Policy knobs (std-like, must be explicit)

Document in code and docs:

- **Initial bucket count** and **growth** (next prime, power-of-two with mix, etc.).
- **`max_load_factor`** for chaining (often **1.0** or lower).
- **Rehash** when `size / bucket_count` exceeds **alpha**.
- **Hash function + mix** alignment with existing **`HashUtils`** where applicable.

Do **not** claim bitwise identity with libstdc++/libc++/MSVC; claim **behavioral** parity where true (average complexity, invalidation rules you choose to support).

### 6.5 Read path: complexity and locality

| Operation | Expected |
|-----------|----------|
| **find** | O(1) average; O(N) worst if all keys in one bucket |
| **iterate all** | O(B + N) over buckets and chains |

**vs flat `items` + bucket ranges (current read layout):**

- Chaining: **more pointer/index hops**, often **worse cache** on **find** and **iteration**.
- Mitigations: **SoA** for `next`, **small-vector** first collisions in bucket (advanced), optional **iteration list** (insertion order) for fast full scans (extra index per node).

Choose whether **find** or **full iteration** is the primary reader benchmark.

### 6.6 Writer vs reader layout (two sub-options)

1. **Chaining is the final mmap format**  
   - Simplest story for **one** representation.  
   - Accept iteration/find locality tradeoffs; add **iteration index** if needed.

2. **Chaining while mutable; finalize converts to a flatter reader layout**  
   - **Theta(N)** once at finalize instead of **Theta(N)** per insert.  
   - Best **read** locality if flat/range layout wins benchmarks.

Both are compatible with **Option C**; the decision is **benchmark-driven**.

### 6.7 Semantics and stability (document explicitly)

- **Iterator / pointer stability** vs **rehash** (same spirit as STL: rehash may invalidate; erase invalidates erased only — specify precisely for blob pointers).
- **Value / key addresses** in the arena may move on **node pool realloc** or **rehash** if nodes are physically moved — align with existing Zmeya rules about **invalid raw pointers across growth**.

### 6.8 Arena / compaction strategies (pair with implementation)

- **Defer** full arena compaction until **finalize** while heavily mutating hashes, **or**
- **Register** every **`roffset`** edge touched by mutation (expensive), **or**
- **Index pool + single O(N) fixup** after compact.

Pick one strategy early; mixing without discipline causes bugs.

---

## 7. Suggested implementation order

1. **Read-only** chained layout on mmap: **`find`**, **iteration**, tests (prove format + correctness).
2. **Builder:** incremental **insert / erase / rehash** using **index pool + buckets** without calling **`impl_assign_*`** per operation.
3. **Compaction:** integrate chosen strategy (defer / fixup / registry).
4. **Optional:** **finalize flatten** or **dual encoding** if read benchmarks favor today's flat **`items`** layout.
5. **Benchmarks:** incremental insert curve vs **bulk `assign`**; mmap **find** and **scan**.

---

## 8. Complexity reference table (current vs target)

| Area | Current (incremental hash APIs) | Risk | Target (Option C + real incremental impl) |
|------|----------------------------------|------|---------------------------------------------|
| `hashmap_insert` (new key) | Theta(n) per call | Critical | Amortized O(1) |
| `hashset_insert` | Theta(n) per call | Critical | Amortized O(1) |
| `hashset_erase` / `hashmap_erase` | Theta(n) per call; string path heavy alloc | Critical | Amortized O(1) average |
| `finalize` | Theta(live) + Theta(R log P) + prune Theta(R * D) | Major in edge cases | Same order class; fewer dead slabs if fewer rebuilds |
| Read `find` | O(1) avg, O(n) worst | Major if skewed | O(1) avg, O(n) worst (same class) |
| Read full scan | Good flat locality | N/A | May need iteration assist |

---

## 9. Top practical fixes (current codebase, before or in parallel with Option C)

These remain valid even while designing Option C:

1. **Batch** mutations in **`std::unordered_map` / `set`** and **single `assign`** where incremental APIs are still the old implementation — **low effort**, **immediate** relief.
2. **Avoid** chatty **`string_append`** for large strings; build **`std::string`** then **`assign`** once.
3. **Consider** API to **avoid copying** finalized blob into **`std::vector`** (move / borrow) for large outputs.
4. **Profile** **`finalize`** if **R** (registry size) and **D** (dead segments) explode — improve prune or reduce churn.

---

## 10. File references (codebase)

| Topic | Files |
|-------|--------|
| Incremental hash snapshot/rebuild | `Zmeya/ZmeyaSerializeApi.inc` |
| Bulk hash layout build | `Zmeya/ZmeyaBuilderAssignImpl.inc` |
| Arena, string append, finalize | `Zmeya/ZmeyaBuilderBase.inc` |
| Read `HashMap` / `HashSet` | `Zmeya/ZmeyaHashMap.h`, `Zmeya/ZmeyaHashSet.h` |
| `write_blob` return copy | `Zmeya/ZmeyaBlobWriter.inc` |

---

## 11. Conclusion

The **current** incremental hash path is **slow by design** (full rebuild per operation), not because **chaining vs flat buckets** is theoretically faster. **Option C** with **index-based chaining**, explicit **rehash/load-factor policy**, and a clear **compaction strategy** is a viable **best-effort** direction for **both** efficient mutation and **mmap** reads, at the cost of **substantial** implementation work and **benchmark-driven** tuning of the **final** serialized shape (pure chaining vs **finalize flatten**).

---

*Document generated from engineering review and design discussion; update as the format and APIs land.*

# 07: Incremental **`HashSet`** and **`HashMap`** (Q1)

## Goal

Expose **std-like** mutation for **`zm::HashSet`** and **`zm::HashMap`** during **`write_scope`**: **insert**, **emplace**, **erase**, **clear**, **find** (read already exists).

## Current state

- Bulk **`assign`** from **`std::unordered_*`** builds dense arrays + bucket structure in one shot.
- Layout uses nested **`Array`** of entries + bucket metadata (see **`Zmeya.h`** **`HashMap`/`HashSet`**).

## Strategy forks

### A: Rebuild-on-write (simplest)

Each mutating operation:

1. Decodes current logical contents (or walks existing zm structure).
2. Builds new **`std::unordered_*`** (or vector + sort) in C++ heap.
3. Calls existing **`assign`** to emit fresh zm layout.

**Pros:** Reuses tested **`assign`**; fewer registration bugs.  
**Cons:** **O(n)** per op; acceptable for small tables or prototyping.

### B: Incremental structure mutation

Mutate backing **`Array`** segments and bucket arrays in place; update registration for every moved **`Pointer`**.

**Pros:** Better asymptotics.  
**Cons:** High complexity; easy to break hash invariants.

**Recommendation for first mergeable milestone:** **A** behind a **`detail`** flag or as default until **B** is proven. Document performance characteristics.

## Registration requirements

- Every **`Pointer`**, **`Array`**, nested **`String`** inside hash layout must go through **same registration** as **06**.
- **Seal** must see complete edge set (**04**).

## Tests

1. Interleave **insert / erase** vs golden **bulk build** from equivalent **`std::unordered_*`** - **identical** serialized bytes (or document ordering differences if hash iteration order differs - **prefer deterministic finalize**: sort entries by key for comparison tests only, not necessarily on-wire order).

**Note:** **`std::unordered_map`** iteration order is nondeterministic; **golden compare** should compare **logical multiset of entries** or **canonical reorder** of serialized structure if format is order-sensitive.

## Dependencies

- **06** (array primitives stable).
- **02**, **03**, **04**.

## Deliverables

- Public methods on **`HashMap`/`HashSet`** under **`#ifdef ZMEYA_ENABLE_SERIALIZE_SUPPORT`** (mirror **`Array`** pattern).
- Tests in **`ZmeyaTestNewAPI.cpp`** or new file **`ZmeyaTestIncrementalHash.cpp`**.

## Risks

- **Determinism:** Hash seed / bucket layout must match **`assign`** path for same keys if byte-identical output required (**Q10**). May require **shared subroutines** between **`assign`** and incremental paths.

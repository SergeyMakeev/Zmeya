# 06: Incremental **`Array`** and **`String`** (Q1 partial)

## Goal

**Normative:** **`zm::Array`** and **`String`** support **std-like** incremental mutation during **`write_scope`** (**`docs/ZmeyaWritePathDesign.md` Q1**): grow, shrink, append, erase (subset as needed for v1).

## Current constraints

- **`assign(Array&, vector)`** asserts **second full assign** forbidden (**fragmentation**).
- No **`push_back`** on **`zm::Array`** in builder context.

## API sketch (choose names during implementation)

| Operation | **`Array<T>`** | **`String`** |
|-----------|----------------|---------------|
| Append one | **`push_back(T)`** or **`emplace_back(...)`** | **`append(const char*)`**, **`operator+=`** |
| Erase | **`erase_at(index)`**, **`clear()`**, **`pop_back()`** | **`clear()`** |
| Resize | **`resize(n)`**, **`reserve(n)`** | reserve if useful |

All must:

1. Run only under active builder (**serialization enabled**).
2. **Register** new slots / slab edges (**02**).
3. Participate in **realloc sweep** (**03**).
4. Leave **final seal** consistent (**04**).

## Algorithm outline

### **`push_back`**

1. If empty: allocate first slab (like **`assign`**).
2. Else if **`numElements < capacity`** (need slab metadata): bump allocator extends slab **or** realloc slab copy (bump-only cannot truncate slab without holes - **append-only slab growth** by reallocating new bigger slab and copying - acceptable under bump-only **without free**, leaks old slab bytes until optional compaction end goal).

**Important:** Without **free**, repeated slab growth leaves **dead bytes** in arena. **Acceptable for v1 per Q2**; document **reserve** for users.

### **`String::append`**

- Similar to **`push_back`** for char runs; coalesce memcpy.

## Interaction with single-assign assertion

- Replace assertion with: **either** first **`assign` from `vector`** **or** incremental API, **not** mixed arbitrarily if that complicates invariants; **or** define **reset()** that clears and allows **`assign`** again.

## Tests

1. Build identical logical array via **`assign(vector)`** vs loop **`push_back`** - **same final bytes** after **`finalize`**.
2. Stress: many **`push_back`** with tiny initial **`write_scope`** reserve - triggers **realloc** + registry correctness (**03**).

## Dependencies

- **01-05** in place for registration, reloc, seal.

## Risks

- **O(n^2)** slab copies if naive growth; use **geometric growth** for slab size (implementation detail).

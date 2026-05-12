# 02: Registration and parallel metadata (Q3, Q5)

## Goal

- **Q5:** Every mutable **indirection slot** ( **`Pointer::relativeOffset`**, **`Array`** header fields, **`String::data`**, hash nested **`Array`** headers ) **registers** with the writer when produced through supported APIs.
- **Q3:** Non-final bookkeeping lives in **parallel structures** keyed by stable slot identity, **not** extra persistent fields in on-disk headers.

## Current state

- **`assign`** writes **`relativeOffset`** / **`numElements`** directly into **`data[goffset]`** via **`get_ptr_unsafe_to_store`**.
- No registry of **which** offsets must be rewritten if memory moves or if seal produces a second buffer.

## Data structures (proposal)

### Slot identity

Use **`goffset_t slot_offset`** = global byte offset of the **start of the header field** that will contain final **`roffset_t`** (for **`Pointer`**, the **`relativeOffset` word**; for **`Array`**, register **both** **`relativeOffset`** and **`numElements`** locations if both can change independently).

**Invariant:** **`slot_offset`** is stable across **`vector`** realloc **as an integer** (same index into the logical arena). When **`data.data()`** moves, **slot_offset** still refers to the same index in the **`vector`** (it does **not** move numerically).

### Registry entry (minimal)

Per registered word (or per logical edge):

- **`slot_offset`**: **`goffset_t`**
- **`kind`**: enum **`PointerTarget`**, **`ArrayData`**, **`ArrayLength`**, **`StringData`**, etc.
- **Build-time target** (parallel metadata): e.g. **`goffset_t target_goffset`** or **`Span`** descriptor stored **only in writer tables** until seal converts to self-relative.

**Note:** For **`Array`**, the **element slab** may be a separate allocation range; registration must include **edges from header to slab** and **edges inside slab** (e.g. **`Pointer`** elements).

### Storage

- **`std::vector<RegistryEntry>`** on **`BuilderBase`** (or nested **`SlotRegistry`** type).
- Consider **sorted by `slot_offset`** for binary search when patching after compaction (future).

## Hook points

| Operation | Registration action |
|-----------|----------------------|
| **`placementCtor<Pointer<T>>`** | Register **`relativeOffset`** slot; initialize parallel target to null / zero. |
| **`assign(Pointer&, T*)`** | Update parallel target **and** optionally defer writing **`relativeOffset`** until seal, **or** write provisional relative offset for in-session **`get()`** (see **04-seal-two-phase**). |
| **`assign(Array&, vector)`** | Register header slots; register slab range as relocatable region. |
| **`assign(String&, ...)`** | Register **`String::data.relativeOffset`** slot + char span. |
| **`HashMap` / `HashSet` `assign`** | Register all nested **`Array`** / **`Pointer`** sites introduced by layout. |

## Interaction with **Q4**

- Parallel metadata holds **absolute or arena-global targets** during build; **no tag bits** in **`roffset_t`** fields on disk for v1.

## Correctness checklist

1. **Completeness:** Any code path that **`memcpy`** into the arena without ctor must either **forbid** or **manual_register_slot(...)**.
2. **`allocate<TRoot>()`** at root: root object fields created by ctor register automatically.
3. **Nested **`allocate()`**: same.

## Deliverables

- **`BuilderBase::register_slot(...)`** ( **`detail`** ), called from **`assign`** and ctors.
- **Debug build:** **`assert`** that every **`relativeOffset`** write touches a registered slot (optional hash set of allowed offsets).
- Unit tests: register count matches expected for small **`write_scope`** examples.

## Dependencies

- **01-writer-context**: **`BuilderBase*`** available without fragile TLS in tests.

## Risks

- **Performance:** registry push per assign; acceptable for v1 (measure in **`ZmeyaTest`** stress).

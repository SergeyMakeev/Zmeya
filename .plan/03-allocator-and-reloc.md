# 03: Bump allocator and reallocation (Q2, Q8)

## Goal

- **Q2 (initial):** **Bump-only** allocation during construction; **no** user **`free`** in v1. Document that **bump + free + compaction at seal** is the **end goal**.
- **Q8:** When **`std::vector<char> data`** **reallocates**, **every** registered slot and any writer-owned metadata keyed by **old** pointers must be updated.

## Current state

- **`BuilderBase::alloc_aligned`** appends to **`data`** and may trigger **`vector` reallocation**.
- **`get_global_offset(ptr)`** computes offset from **`data.data()`**; after realloc, **same index** still valid, **raw pointers** into old buffer are invalid.

## Phase 1: Bump-only (v1 allocator)

1. Keep **`alloc_aligned`** as the primary bump.
2. Ensure **no** stored **raw pointer** in **`BuilderBase`** except **`data.data()`** used transiently; registry uses **`goffset_t`**, not **`char*`**.

## Phase 2: Realloc hook (mandatory for Q8)

### Option A (preferred): Custom vector allocator / wrapper

- Replace **`std::vector<char, BufferAllocator<char, ...>>`** with a type that **notifies** **`BuilderBase::on_buffer_relocated(char* old_base, char* new_base, size_t cap)`** ... **but** **`std::vector`** does not expose realloc hooks portably.

### Option B: Wrap **`resize` / `reserve` / `push_back`** paths

- Override or funnel **all** mutators that grow **`data`** through **`BuilderBase::grow_arena(...)`** that:
  1. Calls **`data.resize`** / reserve.
  2. If **`data.data()`** changed, calls **`relocate_all_slots(old_base, new_base)`**.

**Detection:** Compare **`data.data()`** before and after growth.

### Relocate algorithm

1. **`old_base`** = saved pointer before operation.
2. After operation, **`new_base = data.data()`**.
3. If **`old_base == new_base`**, return.
4. Else:
   - For **registry**: entries keyed by **`goffset_t`** unchanged; **no update** needed for slot indices.
   - For **parallel metadata** storing **pointer values** (`char*`): **forbidden** in v1 - store only **`goffset_t`**.
   - For **any** internal structure storing **`char*`** or **`T*`**: remap or recompute from **`goffset_t`**.
   - **User raw pointers:** still invalid across realloc (document); **zm::** **`get()`** during build must compute from **self + relativeOffset** using **current** **`data.data()`** - already **position-independent** if **`relativeOffset`** is correct.

**Critical insight:** Self-relative **`Pointer`** fields remain valid across realloc **if** **`relativeOffset`** is correct **and** **`this`** and target move together in the same buffer. Problem: **`relativeOffset`** was computed vs **old** buffer addresses; after realloc, **`this`** address changes so **same numeric **`relativeOffset`** can point to wrong place**.

Therefore **Q8** implies: either

- **During build**, store **parallel target as `goffset_t`** and **do not trust** **`Pointer::get()`** until seal, **or**
- **On realloc**, **rewrite every **`relativeOffset`**** from **`goffset_t` targets** in registry (full patch pass).

**Conclusion:** Phase 2 must implement **realloc-time sweep**: for each **`Pointer`** / **`Array`** slot, recompute **`relativeOffset`** from **`slot_goffset`** and **`target_goffset`** stored in parallel metadata (or from live **`get_global_offset`** of target if target pointer refreshed).

## Implementation steps

1. Add **`BuilderBase::note_arena_moved()`** called from every growth path.
2. Implement **`recompute_relative_offsets()`** using registry (**depends on 02**).
3. Add **stress test**: small initial **`write_scope`** size forcing multiple **reallocs** during **`assign`**; verify final blob matches large-reserve run (**golden**).

## Future: bump + free + holes (**Q2 end goal**)

1. Introduce **free list** or **mark bits** for dead ranges.
2. **Seal** compacts **live** ranges into dense buffer (**04-seal-two-phase**).
3. Registry participates in **remap** during compaction (harder than realloc-only).

## Deliverables

- Centralized arena growth function(s).
- Tests proving correctness under forced realloc.

## Dependencies

- **02-registration** must store **targets** as **`goffset_t`**, not raw pointers, for reloc sweep simplicity.

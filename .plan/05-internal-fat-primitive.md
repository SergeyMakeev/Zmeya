# 05: Internal fat primitive and public names (Q6)

## Goal

- **Q6:** **One internal implementation** for offset + extent semantics shared by **`Pointer`**, **`Array`**, **`String`** plumbing; **public** types remain **`zm::Pointer`**, **`zm::Array`**, **`zm::String`** (typedefs / thin wrappers).

## Current layout (must preserve post-seal)

- **`Pointer<T>`**: **`roffset_t relativeOffset`** only.
- **`Array<T>`**: **`roffset_t relativeOffset`**, **`uint32_t numElements`**.
- **`String`**: **`Pointer<char> data`**.

**sizeof / alignment** after seal must match existing static asserts in **`BuilderBase`** constructor.

## Implementation strategy

### Option A: **`detail::IndirectionSlot`** template family

- **`detail::SpanSlot`**: holds logical **`{ target_goffset, numElements }`** for arrays/strings during build.
- **`zm::Array<T>`** remains layout-compatible: **same two fields** as today; **builder-only** code paths may cast to **`detail::BuildView<Array<T>>`** when **`ZMEYA_ENABLE_SERIALIZE_SUPPORT`** defined.

### Option B: Shared helpers only

- Keep struct layouts unchanged; extract **`namespace detail { void set_array_data(BuilderBase&, Array<T>&, ...); }`** used by **`assign`** and future **`push_back`**.

**Preferred:** **Option B** first (lower ABI risk); evolve toward **A** if duplication explodes during **06**.

## Code consolidation targets

| Location | Consolidation |
|----------|----------------|
| **`assign(Array&, vector)`** | Single **`allocate_elements`** + registry hooks. |
| **`assign(String&, ...)`** | Shared with **`Pointer<char>`** target registration. |
| **`assign` for **`HashMap`/`HashSet`** | Delegate array chunks to shared helpers. |

## Deliverables

- Internal header section (still **`Zmeya.h`** or **`ZmeyaDetail*.h`** if split) with **`detail`** helpers used by **06/07**.
- No change to **mmap** reader code paths for **`const`** **`Array`** / **`Pointer`** / **`String`** getters unless required by incremental mutation.

## Dependencies

- **02-registration** defines what **fat** means for parallel metadata (extent separate from **`numElements`** on wire).

## Risks

- Accidentally breaking **`static_assert(is_trivially_copyable<...>)`**. Mitigation: fat state **only** in **`BuilderBase`** tables, not in **`zm::`** headers.

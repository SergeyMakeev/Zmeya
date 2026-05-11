# 04: Two-phase finalize and compatibility (Q9, Q10)

## Goal

- **Q9:** **Phase 1** produces **final dense layout** (and optional remap). **Phase 2** patches **every registered edge** to **final self-relative** **`roffset_t`** values.
- **Q10:** Output matches **today's** reader layout for the same logical structure: **padding**, **`roffset_t`** semantics, trivial headers.

## Current **`finalize`** (**`BuilderBase::finalize`**)

- Pads **`data.size()`** to alignment.
- Returns **`Span<char>(data.data(), data.size)`**.
- **No** separate patch pass; **`assign`** already wrote self-relative offsets.

## Evolution path

### Stage 0 (bootstrap)

- Registry exists but **parallel targets** mirror what **`assign`** already wrote into slots.
- **`finalize`** remains **padding-only** if slots already contain correct self-relative values **and** no compaction.

### Stage 1 (build-time vs seal-time split)

- During **`assign`**, optionally write **placeholder** or **last-known-good** relative offset for **`get()`** during construction; **authoritative** targets live in **parallel metadata**.
- **`finalize` Phase 1:** If arena == final (no compaction), **memcpy** or **in-place** finalize padding.
- **`finalize` Phase 2:** Walk registry; for each slot, **`target_final_goffset`** -> **`get_relative_offset(slot_ptr, target_final_goffset)`**; write **`roffset_t`**.

### Stage 2 (compaction / second buffer)

- **Phase 1:** Allocate **`final_buffer`**, copy **live** segments (order defined by compaction policy); build **`old_goffset -> new_goffset`** map.
- **Phase 2:** For each registry entry, map **slot** and **target** through remap table; compute self-relative; write into **`final_buffer`** at **new** slot positions.

**Ordering:** Patches must use **final** addresses for **`base`** pointer in **`get_relative_offset(base, target)`** (**Q9**). Typical approach: **phase 1** finishes layout; **phase 2** runs with **`final_buffer`** mapped or with **stable slot ids** that resolve to **final** **`goffset_t`**.

## Compatibility verification (**Q10**)

1. **Golden tests:** For fixtures built with **legacy** single-pass **`assign`**, compare **byte string** to **new writer** output (same logical input).
2. **Round-trip:** **`mmap`** / deserialize tests already in **`ZmeyaTest`** must pass unchanged on outputs.

## Edge cases

- **Null **`Pointer`**: **`relativeOffset == 0`**; registry entry should allow **null** without target.
- **`Array`** empty: **`numElements == 0`**, **`relativeOffset`** convention per existing format (verify **0** vs unused).
- **Root at offset 0**: unchanged.

## Deliverables

- **`BuilderBase::finalize`** split into **`finalize_layout`** + **`patch_edges`** or internal steps with clear comments.
- Tests comparing **old vs new** finalize for identical **`write_blob`** scenarios.

## Dependencies

- **02-registration** complete.
- **03** if realloc sweep writes provisional offsets (phase 2 still validates).

## Risks

- **Double application:** Ensure **`patch_edges`** idempotent or run **exactly once**.

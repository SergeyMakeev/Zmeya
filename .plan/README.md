# Zmeya write-path implementation plan

This folder breaks **`docs/ZmeyaWritePathDesign.md`** into executable subplans. The design doc remains authoritative for **Q1-Q11** decisions.

## Central references

| Artifact | Role |
|----------|------|
| `docs/ZmeyaWritePathDesign.md` | Normative requirements (registration, explicit writer, two-phase seal, compatibility). |
| `Zmeya/Zmeya.h` | Current **`BlobWriter`**, **`detail::BuilderBase`**, **`write_scope`**, TLS (**`g_tls_active_builder`**), **`assign`**, **`finalize`**. |
| `NEXT_STEPS.md` | Documented realloc hazards; superseded behaviors should be updated when the new writer lands. |

## Workstream map

```
01-writer-context -----> 02-registration -----> 03-allocator-and-reloc
        |                        |                      |
        +------------------------+----------------------+
                                 v
                    05-internal-fat-primitive (can overlap with 02)
                                 |
                                 v
                    04-seal-two-phase (depends on 02, 03)
                                 |
              +------------------+------------------+
              v                                     v
    06-incremental-array-string           07-incremental-hash
              +------------------+------------------+
                                 v
                    08-testing-migration
```

**Suggested sequencing**

1. **01** (explicit writer handle): Unblocks deterministic tests and removes TLS-only coupling before touching assignment internals.
2. **02 + 03** (registry + bump + realloc fixup): Without these, incremental mutation cannot be safe across **`vector`** growth.
3. **05** (internal fat primitive / slot identity): Can proceed in parallel with **02** once writer API is stable; converges **`Pointer`**, **`Array`**, **`String`** internal paths.
4. **04** (two-phase seal): Initially may be **identity seal** (today **`finalize`** padding only) until build-time fields deviate from final **`roffset_t`** encoding; becomes mandatory when parallel metadata holds targets separately from slots.
5. **06** then **07** (incremental APIs): Arrays/strings first (simpler graph); hash tables depend on array primitives and registration completeness.
6. **08** continuous from day one (golden blob comparison, realloc stress).

## Subplans

| File | Scope |
|------|--------|
| [01-writer-context-and-api.md](01-writer-context-and-api.md) | Replace TLS-as-only-context with explicit **`BlobWriter`** / builder binding; optional TLS shim for backward compatibility during migration. |
| [02-registration-and-metadata.md](02-registration-and-metadata.md) | Slot registry, stable identities (**`goffset_t` slot** or opaque id), parallel metadata tables (**Q3**). |
| [03-allocator-and-reloc.md](03-allocator-and-reloc.md) | Bump-only phase (**Q2**); hook **`std::vector`** realloc to remap registry (**Q8**); notes for future free+compaction. |
| [04-seal-two-phase.md](04-seal-two-phase.md) | Layout phase + patch phase (**Q9**); final bytes match today (**Q10**). |
| [05-internal-fat-primitive.md](05-internal-fat-primitive.md) | Single internal representation; public **`Array`** / **`String`** / **`Pointer`** unchanged at ABI level post-seal (**Q6**). |
| [06-incremental-array-string.md](06-incremental-array-string.md) | **`push_back`**, **`erase`**, resize, string append; interaction with single-assign assertions. |
| [07-incremental-hash.md](07-incremental-hash.md) | **`HashSet`** / **`HashMap`** std-like mutation (**Q1**); strategy options (rebuild vs incremental). |
| [08-testing-and-migration.md](08-testing-and-migration.md) | Regression tests, round-trip vs readers, stress realloc, updating **`NEXT_STEPS.md`**. |

## Cross-cutting rules (do not violate)

- **Q4:** No tag bits inside on-wire offset words in v1.
- **Q10:** Final **`std::vector<char>`** must be readable by existing mmap/deserialize tests unchanged for equivalent logical content.
- **Q5:** Edge list completeness comes from **registration**, not an optional typed walk (walk may exist only as **debug verification** if desired later).

## Definition of done (program-wide)

1. **`write_scope`** (or successor) runs user closure with an **explicit writer**; TLS is not the only discovery path for **`assign`** (**Q7**).
2. **`vector<char>`** reallocation updates **all** registered slots; documented raw-pointer hazards narrowed to unsupported escape hatches (**Q8**).
3. **Finalize** implements **two-phase** semantics required by **Q9** (even if phase 1 is memcpy-only until compaction exists).
4. **Incremental** **`Array`** / **`String`** / hash APIs ship in v1 per **Q1** with tests proving sealed output matches legacy bulk **`assign`** where comparable.
5. **`AGENTS.md`** / **`NEXT_STEPS.md`** updated so future agents do not reintroduce TLS-only assumptions.

## Open implementation choices (not decided in design doc)

Record decisions here as you lock them:

| Topic | Options | Status |
|-------|---------|--------|
| Slot identity | **`goffset_t` of slot** vs opaque **`uint32_t` id** remapped on realloc | **`goffset_t` keys in `std::unordered_map` on `BuilderBase` (parallel target `goffset_t`)** |
| **`BlobWriter` carries `BuilderBase*`** | Yes (minimal) vs wrapper type | **Yes via `builder_base()` -> `impl_`** |
| Hash incremental internals | Full rebuild of backing arrays on each mutation vs optimized paths | **Rebuild-on-write (decode to `std::unordered_*`, `assign`) for v1** |

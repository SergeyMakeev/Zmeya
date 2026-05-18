# 08: Testing, golden blobs, and documentation migration

## Goal

Provide continuous verification for the write-path rewrite without regressing **mmap** readers or existing **`ZmeyaTest`** suites.

## Test layers

### 1. Unit-level (fast)

| Subject | Idea |
|---------|------|
| Registry | After scripted **`assign`**, **`register_slot`** count and coverage match expectation. |
| Realloc | Inject **`shrink_to_fit`**-like or artificial **`reserve(1)`** then growth - **no** UB, final blob valid. |
| Seal phases | **Phase 2** only touches registered offsets. |

### 2. Integration / golden

- For representative **`TRoot`** types: serialize with **legacy path** (single-pass **`assign`**) vs **new path** (incremental + seal) - **memcmp** final **`vector<char>`** where deterministic.

### 3. Existing tests

- Run full **`ZmeyaTest`** (**`AGENTS.md`**): **`cmake --build`** then **`ZmeyaTest.exe`** from **`build`**.
- Fix failures by aligning seal output or adjusting tests if API intentionally changes.

### 4. Stress / fuzz (optional later)

- Random sequences of **`push_back` / erase`** on **`Array`** with tiny initial arena.

## Documentation updates

| File | Action |
|------|--------|
| **`NEXT_STEPS.md`** | Replace **TLS-only** and **raw pointer** warnings with **new contract**: explicit writer, **`goffset_t`** targets, when raw pointers are still unsafe. |
| **`AGENTS.md`** | Mention incremental APIs and **`write_scope`** reserve guidance if still relevant. |
| **`README.md`** | Only if user-facing examples need incremental snippets (**keep minimal per repo norms**). |

## CI expectations

- No new warnings at **MSVC** warning levels used by project.
- **ASCII-only** sources per workspace rules.

## Exit checklist

- [ ] All **`ZmeyaTest`** green on primary configuration (Windows VS per **`AGENTS.md`**).
- [ ] At least one **memcmp** golden test for blob identity (**legacy vs new**).
- [ ] At least one **forced realloc** test (**03**).
- [ ] **`NEXT_STEPS.md`** reflects **Q7-Q8** outcomes.

# Memory Safety Audit

Audit scope: `Zmeya/` library headers (ReadOnly + Serialize), root `ZmeyaTest*.cpp`, `ZmeyaBench.cpp`, `TestHelper.h` as exercised by tests. Third-party `extern/googletest` excluded except where it affects how tests exercise the library.

## Audit revision

- Baseline commit: `d2f8791` -- "Split Zmeya headers into ReadOnly and Serialize folders."
- Impact: File moves and `ZMEYA_ENABLE_SERIALIZE_SUPPORT` guards around mutating APIs. No fundamental change to blob validation logic or arena memcpy/memmove behavior; audit conclusions remain anchored in current `Zmeya/ReadOnly/ZmeyaBlobValidate.h` and read-side view types.

## Executive Summary

Overall risk level: **High** for **untrusted binary blobs** unless callers strictly match the validation contract; **Medium** for **trusted-only** blobs produced by the same Zmeya writer and consumed with correct `TRoot` typing and alignment.

The library is explicitly split between a **fast unchecked read surface** (`zm::Array`, `zm::Pointer`, `zm::String`, etc.) and an optional **bounded validator** (`validate_blob_view` / `as_root_blob`). The dominant failure mode is **treating external bytes as a `TRoot` without validation**, or validating with a **composite root type** where deep layout checks are **not** applied. Secondary risks: **unchecked indexing** on read paths, **non-portable stack-pointer rejection** on non-Windows builds, and **thread-local write context** misuse across threads.

## Highest-Risk Findings

### Finding: Composite `TRoot` skips deep blob validation

**Severity:** High  
**Category:** API design / asset lifetime (untrusted input contract)  
**Location:** `Zmeya/ReadOnly/ZmeyaBlobValidate.h` -- `validate_blob_view`, `BlobLayoutValidator::field_dispatch`, `shallow_root_in_span`  
**Problem:** When `TRoot` is not classified as a "direct" Zmeya container (`is_direct_zm_container_v`), `validate_blob_view` only checks that `sizeof(TRoot)` fits in the span and meets alignment. It does **not** recursively walk user-defined struct members that embed `zm::HashMap`, `zm::Array`, `zm::Pointer`, etc.  
**Why it is dangerous:** A crafted blob can satisfy the shallow check while inner self-relative fields point outside the span or form invalid chains. Subsequent reads use unchecked `reinterpret_cast` and pointer arithmetic.  
**How to catch earlier:** Treat `validate_blob_view<MyComposite>()` as insufficient for untrusted input; add an application-level walk, flatten roots, or extend validation for your concrete `TRoot` layout.  
**Runtime defense:** Always call a validator that matches the real layout; prefer `as_root_blob` only after confirming coverage; combine with process sandboxing and size caps.  
**Recommended fix:** Document the limitation prominently; consider a code-generated or macro-driven field walk for registered root types, or a separate `validate_blob_view_deep<TRoot>>` that reflects the schema.  
**Test coverage needed:** Fuzz tests where `TRoot` is a struct wrapping `zm::HashMap` / `zm::Array` and assert that **shallow-only** validation incorrectly returns Ok (regression test for docs) or add deep validation and assert rejection.

---

### Finding: Read-side `Array::operator[]` has no bounds check

**Severity:** High (when validation skipped or incomplete)  
**Category:** memory stomp (read OOB) / buffer safety  
**Location:** `Zmeya/ReadOnly/ZmeyaArray.h` -- `operator[]` vs `at()` / `try_at()`  
**Problem:** `operator[]` indexes `data[index]` with no check against `numElements`. `at()` uses `ZMEYA_ASSERT`; `try_at` is safe but optional.  
**Why it is dangerous:** Corrupt `numElements` or stale use after partial validation yields **out-of-bounds read** (info leak / crash), not caught in Release if callers used `operator[]`.  
**How to catch earlier:** Prefer `try_at` or `at` for external data; static analysis on call sites; fuzzing.  
**Runtime defense:** UBSan/ASan on test binaries; never skip `validate_blob_view` for untrusted spans.  
**Recommended fix:** For hardened builds, consider a macro-guarded `operator[]` that checks bounds, or document that `operator[]` is **trusted-blob-only** and steer external parsers to `try_at`.  
**Test coverage needed:** Malformed array headers with huge `numElements` after partial validation paths.

---

### Finding: `Pointer::get()` and `zm::String` navigation are unchecked on the hot path

**Severity:** Medium (elevated to High if validation omitted)  
**Category:** buffer safety / use-after-free (only if blob memory is unmapped while pointers retained)  
**Location:** `Zmeya/ReadOnly/ZmeyaPointer.h` (`get`, `operator->`, `operator*`), `Zmeya/ReadOnly/ZmeyaString.h` (`c_str` -> `data.get()`)  
**Problem:** `get()` computes `toAbsoluteAddr` without span bounds. `try_get_in_blob` exists but is not used by `get()`. `operator->` / `operator*` route through `getUnsafe()` with `ZMEYA_ASSERT(relativeOffset != 0)` only, not span checks.  
**Why it is dangerous:** Dereferencing a `Pointer` that points out of the backing mapping is UB. `String::c_str()` follows the same address chain.  
**How to catch earlier:** Lint for `->`/`*` on `zm::Pointer` in code handling external blobs; wrap accesses with `try_get_in_blob`.  
**Runtime defense:** Full blob validation before any traversal; keep mmap lifetime >= all `const TRoot*`.  
**Recommended fix:** For external blobs, provide thin accessors that take `(blob_begin, blob_size)` or only expose `try_get_in_blob` patterns in examples.  
**Test coverage needed:** Pointers deliberately aimed one byte past span end; should fail validation and never reach `get()`.

---

### Finding: `is_stack_pointer` is a no-op off Windows

**Severity:** Medium  
**Category:** API design / portability gap  
**Location:** `Zmeya/Serialize/ZmeyaSerializeFoundation.h` -- `is_stack_pointer`  
**Problem:** On non-Windows platforms the function always returns `false`, so `get_relative_offset` cannot reject stack addresses the way it can on Windows (per `AGENTS.md`).  
**Why it is dangerous:** Integration mistakes (passing stack `&x` into relative offset registration) may persist until **finalize** or later `ZMEYA_ASSERT`, or yield corrupted blobs without an early deterministic error.  
**How to catch earlier:** CI on Windows for serialize tests; code review for `get_relative_offset` uses.  
**Runtime defense:** Expand platform hooks (pthread stack bounds, `/proc/self/maps` heuristics) if Linux/macOS safety parity matters.  
**Recommended fix:** Document asymmetry; add optional Linux stack-limit query behind a macro.  
**Test coverage needed:** Cross-platform matrix documenting expected `is_stack_pointer` behavior (already partially noted in AGENTS.md).

---

### Finding: Write path relies on `memcpy`/`memmove` and raw slab pointers

**Severity:** Medium (trusted builder invariants)  
**Category:** memory stomp / ownership ambiguity  
**Location:** `Zmeya/Serialize/ZmeyaBuilderBaseArrays.inc`, `ZmeyaBuilderBaseStrings.inc`, `ZmeyaBuilderHashChainNodesGrow.inc`, `ZmeyaSerializeApiAssignDetail.inc`  
**Problem:** Heavy use of `get_ptr_unsafe_to_store`, `reinterpret_cast`, and `memcpy`/`memmove` for growth and relocation. Correctness depends on internal offset bookkeeping and re-binding pointers after growth (documented in `ZmeyaBuilderHashChain.inc` comments).  
**Why it is dangerous:** A bug in offset math or stale cached raw pointers across growth is a classic **heap buffer overflow** in the arena vector.  
**How to catch earlier:** Stress tests with tiny initial reserves (`write_scope_stressed` per AGENTS.md); assertions already dense (`ZMEYA_ASSERT`).  
**Runtime defense:** ASan on unit tests; fuzz incremental hashmap/array ops.  
**Recommended fix:** Keep invariant comments at growth sites; consider sanitizer CI job.  
**Test coverage needed:** Expand stressed realloc paths for string append and hash node growth interleavings.

---

### Finding: `toAbsoluteAddr` does not detect uintptr_t wraparound

**Severity:** Low  
**Category:** buffer safety (edge arithmetic)  
**Location:** `Zmeya/ReadOnly/ZmeyaTypes.h`  
**Problem:** `base + ptrdiff_t(offset)` has no overflow check.  
**Why it is dangerous:** Extreme `roffset_t` values could wrap and land inside the span, evading simple range checks that assume monotonic addressing. Many validator branches reject impossible sizes first, but this is not a universal proof.  
**How to catch earlier:** Saturating or two-sided range checks in validator when combining base and offset.  
**Runtime defense:** Depth- and size-bounded validation (partially present via chain step caps).  
**Recommended fix:** Add explicit `base`/`offset` range check before forming `addr` in validator helpers.  
**Test coverage needed:** Crafted offsets near `INT32_MAX` against small blobs.

---

## Compile-Time Safety Improvements

- Mark more accessors `[[nodiscard]]` where ignoring results implies bugs (several already use `ZMEYA_NODISCARD`).
- Consider `delete`d `Pointer::operator->` in TUs that define a macro for "safe blob build" forcing `try_get_in_blob` ( invasive; may remain documentation-only ).
- `static_assert` already guards trivially-copyable blob fields in parts of the builder (`ZmeyaBuilderBaseCtor.inc`).

## Runtime Debug Defenses

- Default `ZMEYA_ASSERT` maps to `ZMEYA_HARD_ASSERT` (`Zmeya/ReadOnly/ZmeyaConfig.h`); stripping asserts in Release weakens internal guardrails -- project docs already warn about this.
- `BlobLayoutValidator` uses bounded hash-chain walks, visit bitmaps, and nil handling (`Zmeya/ReadOnly/ZmeyaBlobValidate.h`).
- `validate_blob_view` returns structured `BlobViewError` for alignment and span issues before deep walks.

## Asset / Resource Lifetime Risks

- mmap / file-backed blobs (`ZmeyaTest10.cpp`, `ZmeyaBench.cpp`, coverage tests): any `const TRoot*` or raw interior pointer becomes invalid after `UnmapViewOfFile` or buffer free. The library does not pin lifetimes; callers own the backing storage.
- Finalized `BlobBuffer` moved out of `write_scope` remains valid until destroyed; interior pointers are relative to that buffer.

## Concurrency Lifetime Risks

- `detail::g_tls_active_builder` (`Zmeya/Serialize/ZmeyaSerializeFoundation.h`) ties incremental mutation to one builder per thread. Using `zm::Array::push_back` from another thread without external synchronization is undefined.
- `ScopedBuilder` restores previous TLS; nested scopes are supported when balanced.

## Memory Stomp / Buffer Safety Risks

- Extensive `reinterpret_cast` from arena bytes is inherent to the design; mitigated by validation **when used as intended**.
- `Zmeya/ReadOnly/ZmeyaHash.h` uses `memcpy` for aligned 8-byte hashing chunks with length-guarded loop -- caller-supplied `len` must match accessible memory (call sites from string/hash adapters must respect NUL-terminated or bounded string rules from validated blobs).
- Hash table `memmove` in `ZmeyaBuilderBaseArrays.inc` assumes `index` and `sizeOfT` product stays within allocated array slab (builder invariants + asserts).

## Test Coverage Gaps

- Fuzz `validate_blob_view` with random bytes for multiple `TRoot` shapes (especially composite structs).
- Negative tests for `operator[]` with corrupt `numElements` after intentional **bypass** of validation (documents hazard).
- Cross-platform behavior tests for `is_stack_pointer` expectations.
- Incremental serialize fuzzing: interleaved `hashmap_insert`, `string_append`, `array_push_back` under forced realloc (`write_scope_stressed`).

## Refactoring Recommendations

1. Must fix immediately: **Do not ship parsers that accept network/file blobs with composite `TRoot` unless you add layout validation that matches every embedded `zm::` field** (or flatten the schema).
2. Should fix soon: Hardening guide listing **which APIs are unchecked** (`operator[]`, `Pointer::get`, `c_str`) and the **single supported entry** (`as_root_blob` / `validate_blob_view`).
3. Nice to have: Optional hardened accessors; portable stack detection; explicit overflow checks in `toAbsoluteAddr` for validator.

## Final Verdict

- Safe enough to ship under what contract? **Yes for trusted blobs** (same writer version, integrity-checked transport, correct `TRoot` and alignment) especially after `validate_blob_view` when `TRoot` is itself a zm container or you accept shallow-only roots by design.
- What must be fixed before shipping untrusted / external input? **Either restrict `TRoot` to types fully covered by `field_dispatch` from the root, or implement additional validation**; never use `reinterpret_cast` directly on external bytes; cap input size; validate before any `operator[]`/`get()` traversal.
- What to monitor or harden later? Sanitizer CI, fuzzing, Linux stack guard parity, and composite-root schema tooling.

**Bottom line:** The codebase separates **fast unchecked views** from a **real but intentionally shallow validator**; treating those views as safe on arbitrary bytes without matching validation depth is the primary memory-safety failure mode.

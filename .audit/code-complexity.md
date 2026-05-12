# Zmeya (library + tests) code complexity and maintainability audit

Scope: project source under `Zmeya/`, root `ZmeyaTest*.cpp`, `ZmeyaBench.cpp`, `TestHelper.*`. Excludes `extern/`, local `build*` trees, and generated CMake probe TUs.

## Executive Summary

The library is a header-only write/read blob system split across many `.inc` fragments, with correctness-critical pointer re-derivation around arena growth and a **thread-local active builder** for the write path. That split is workable but raises navigation cost. Tests concentrate a lot of behavior in **`ZmeyaTestCoverage.cpp` (~1650 lines)**, which is the largest single maintainability drag.

Top five fixes (ordered):

1. **Split `ZmeyaTestCoverage.cpp`** into several TUs (e.g. strings, arrays, hash maps/sets, deep copy, versioning) or extract shared helpers/fixtures to a `TestCoverageCommon.*` to cut scroll-and-merge pain.
2. **Remove or implement `ZMEYA_VALIDATE_HASH_DUPLICATES`** in `ZmeyaConfig.h` (currently defined in Debug builds but never referenced); either wire checks into hash insert/rehash or delete the macro to stop implying behavior that does not exist.
3. **Add a short "read order" map** at the top of `ZmeyaSerializeApi.inc` (and optionally `ZmeyaBuilderBase.inc`) listing which `.inc` owns which concern so new contributors do not grep-blind.
4. **Reduce test-only dependence on `zm::detail::write_scope_with_initial_buffer_bytes`** where possible by a tiny test-only public wrapper in a `ZmeyaTestSupport` header (same symbol behavior, clearer intent than reaching into `detail` from many TUs).
5. **Long-term, optional:** factor shared logic between `hashset_rehash_impl` and `hashmap_rehash_impl` in `ZmeyaBuilderHashChain.inc` only if you can do it without obscuring the `hm_g`/`hs_g` rebind rules (high regression risk).

## Severity Legend

- **Critical:** likely wrong behavior, security/read-path holes, or change breaks correctness silently.
- **High:** large ongoing cost (time to change, bug risk) or misleading API/docs.
- **Medium:** real friction for mid-level maintainers; fix is bounded effort.
- **Low:** polish, small deletions, or accepted complexity with clear docs.

## Findings

### Finding: Monolithic coverage test translation unit

**Severity:** High  
**Location:** `ZmeyaTestCoverage.cpp`, full file (~1650 lines)  
**Problem:** Dozens of root structs, helpers, and tests live in one file. Reviews and merges conflict often; behavior is hard to discover without search.  
**Why it matters:** Test code sets expectations for API usage; when it is opaque, production call sites drift and regressions hide in noise.  
**Recommendation:** Split by domain (containers, strings, pointers, hash, deep copy, versioning) or introduce shared static helpers in a separate header/cpp pair included by smaller test files. Keep `gtest` filter names stable if CI filters by suite.  
**Confidence:** High

### Finding: Write path split across many include fragments

**Severity:** Medium  
**Location:** `ZmeyaSerializeApi.inc` pulls `ZmeyaSerializeApiAssignDetail.inc`, `ZmeyaBuilderHashChainNodesGrow.inc`, `ZmeyaSerializeApiAssign.inc`, `ZmeyaSerializeApiIncremental.inc`, `ZmeyaSerializeApiInstMembers.inc`, `ZmeyaSerializeApiBlobWriter.inc`; `ZmeyaBuilderBase.inc` pulls arena/registry/alloc/ctor/strings/arrays/finalize; `ZmeyaBuilderHashChain.inc` is separate from `ZmeyaBuilderBase.h`.  
**Problem:** Understanding one feature (e.g. string assign) requires hopping files; there is no single "story" TU.  
**Why it matters:** Mid-level maintainers pay a high context-switch tax; refactors can miss a fragment.  
**Recommendation:** One short comment block listing fragment responsibilities and typical debug order (e.g. assign detail -> builder base strings -> hash chain). Avoid adding new abstraction layers; prefer a README section in code comments only if kept short.  
**Confidence:** High

### Finding: Thread-local active builder is easy to misuse from call sites

**Severity:** Medium (by design, but high cognitive load)  
**Location:** `ZmeyaSerializeFoundation.h` (`g_tls_active_builder`, `ScopedBuilder`, `get_global_builder` / `set_global_builder`); documented in `AGENTS.md`.  
**Problem:** Member mutators on `zm::Array` / `zm::HashMap` etc. reach TLS implicitly; raw pointers into the arena go stale on growth.  
**Why it matters:** Correctness depends on conventions documented outside the type system; mistakes are runtime asserts or memory corruption, not compile errors.  
**Recommendation:** Keep TLS (nested `assign_string_std` needs it) but treat `BlobWriter` + explicit `BuilderBase&` overloads as the primary story in docs; consider `[[deprecated]]` only if you have a migration story (probably not worth it). Optional: assert depth in Release for `ScopedBuilder` imbalance behind a macro (already partially there for `ZMEYA_DEBUG_TLS_BUILDER_STACK`).  
**Confidence:** High

### Finding: `ZmeyaBuilderHashChain.inc` is a large mixed template implementation

**Severity:** Medium  
**Location:** `ZmeyaBuilderHashChain.inc` (~640 lines), symbols such as `BuilderBase::hashset_rehash_impl`, `BuilderBase::hashmap_rehash_impl`, insert/erase paths.  
**Problem:** Rehash, bucket relinking, and `goffset_t` rebind patterns repeat; long functions with multiple growth points.  
**Why it matters:** Any edit risks subtle relocation bugs; reviewers must re-verify `p = get_ptr_unsafe_to_store(hm_g)` discipline throughout.  
**Recommendation:** Do not "clean up" casually. If splitting, extract only mechanical helpers (e.g. "collect chain indices in deterministic order") with tests unchanged. Add a one-screen comment at top referencing the arena invalidation rule already present in the file header.  
**Confidence:** High

### Finding: `assign_string_std` packs several policies in one long function

**Severity:** Medium  
**Location:** `ZmeyaSerializeApiAssignDetail.inc`, `assign_string_std` / `assign_string_cstr` (roughly tens of lines each, multiple branches, capacity growth, dead blob registration).  
**Problem:** Mixed responsibilities: TLS scoping, empty clear, old blob accounting, growth strategy, memcpy, registry updates.  
**Why it matters:** String assign is hot and bug-prone; deep nesting makes review harder than necessary.  
**Recommendation:** Prefer guard clauses and small `static` helpers in the same anonymous namespace/detail (same TU) for "note dead existing", "compute alloc size", without changing behavior.  
**Confidence:** Medium

### Finding: `BlobWriter` methods are mostly one-line forwards

**Severity:** Low  
**Location:** `ZmeyaSerializeApiBlobWriter.inc` (e.g. `hashmap_insert` -> `zm::hashmap_insert(*impl_, ...)`)  
**Problem:** Duplicates the free-function surface; looks like useless wrapping at first glance.  
**Why it matters:** Slight indirection cost is negligible; the real issue is two mental models ("use `w.`" vs "use `zm::` with builder ref").  
**Recommendation:** Keep as-is for discoverability (already stated in `AGENTS.md`); optionally add one line in `AGENTS.md` that free functions are the implementation and `BlobWriter` is the ergonomic facade.  
**Confidence:** High

### Finding: Tests reach `zm::detail::write_scope_with_initial_buffer_bytes` frequently

**Severity:** Low  
**Location:** `ZmeyaTestIncremental.cpp`, `ZmeyaTestCoverage.cpp`, `ZmeyaTest10.cpp`, `ZmeyaTest11.cpp`, `ZmeyaTestNewAPI.cpp`  
**Problem:** Stressing reallocations is good, but `detail::` signals unsupported internals while being widespread.  
**Why it matters:** New tests copy the pattern; if `detail` changes, churn spreads.  
**Recommendation:** Thin `test_write_scope_stressed` inline in `TestHelper.h` that forwards to `detail::write_scope_with_initial_buffer_bytes` with a comment "tests only".  
**Confidence:** Medium

### Finding: `BlobLayoutValidator` is inherently large

**Severity:** Low (complexity justified)  
**Location:** `ZmeyaBlobValidate.h` (~500 lines), `BlobLayoutValidator`, `validate_blob_view`  
**Problem:** Heavy `if constexpr` / trait dispatch for nested layouts.  
**Why it matters:** High line count is not optional for untrusted-input validation without a separate schema language.  
**Recommendation:** Resist splitting unless you also gain test isolation; prefer table-driven error reporting tests if gaps appear.  
**Confidence:** Medium

### Finding: `is_stack_pointer` is Windows-only effective

**Severity:** Low  
**Location:** `ZmeyaSerializeFoundation.h`, `is_stack_pointer`  
**Problem:** Non-Windows builds always return false; relative-offset-from-stack mistakes are not caught there.  
**Why it matters:** Cross-platform developers may assume parity.  
**Recommendation:** Document in `AGENTS.md` or next to the function that Linux/macOS disable the check by policy (conservative vs perf/portability trade-off).  
**Confidence:** High

## Dead Code Candidates

| Location | Why unused / suspect | Safe to delete? | Verification |
|----------|----------------------|-----------------|---------------|
| `ZMEYA_VALIDATE_HASH_DUPLICATES` in `ZmeyaConfig.h` | Macro defined in `_DEBUG` but no `#ifdef` consumer in repo | Likely safe to remove, or implement checks | `rg VALIDATE_HASH_DUPLICATES` only hits the define |
| `ZMEYA_DEBUG_TLS_BUILDER_STACK` | Only toggles extra TLS depth tracking in `ScopedBuilder` | Keep; not dead, optional | Define in CI matrix if you rely on it |
| `std::hash<zm::String>` in `ZmeyaStdHash.h` | No direct mention in repo tests; umbrella include pulls it | Do not delete without checking downstream users | Public STL interop; absence in this repo does not prove unused externally |

## Complexity Hotspots

| Hotspot | Why complex | Simplification |
|---------|-------------|----------------|
| `ZmeyaBuilderHashChain.inc` | Rehash + chain surgery + relocation discipline | Comments + tiny extracted helpers only; avoid big merges of set/map paths |
| `ZmeyaSerializeApiAssignDetail.inc` | String/pointer assign and registry side effects | Guard clauses / local helpers; no new abstraction types |
| `ZmeyaBlobValidate.h` | Full recursive layout proof for untrusted bytes | Keep monolith unless splitting improves tests |
| `ZmeyaTestCoverage.cpp` | Many scenarios in one TU | Split files or shared fixture header |

## Duplicate Patterns / Multiple Ways To Do The Same Thing

| Pattern A | Pattern B | Which to keep | Notes |
|-----------|-----------|-----------------|-------|
| `BlobWriter<Root>::hashmap_insert` | `zm::hashmap_insert(builder, ...)` | Both | `AGENTS.md` already prescribes `BlobWriter` for app code and free functions for explicit builder |
| `zm::write_scope` | `zm::detail::write_scope_with_initial_buffer_bytes` | Public vs test stress | Document test wrapper pattern; keep detail for forced realloc coverage |
| Member `HashMap::insert` (TLS) | Free `hashmap_insert` | Both | Same underlying path; member needs write_scope |

## Useless Abstractions / Wrappers

| Location | Verdict |
|----------|---------|
| `ZmeyaSerializeApiBlobWriter.inc` thin templates | **Keep** -- documents intent at call site |
| `ZmeyaBuilder.h` / `ZmeyaBuilderBase.h` as one-line include shells | **Keep** -- standard header-only composition pattern |
| `ZmeyaSerializeApi.h` wrapping `ZmeyaSerializeApi.inc` | **Keep** -- isolates namespace |

## Large Function / Large File Review

| Location | Approx size | Mixed responsibilities | Split suggestion |
|----------|-------------|------------------------|------------------|
| `ZmeyaTestCoverage.cpp` | ~1650 lines | Everything coverage-related | Split by subsystem or extract helpers |
| `ZmeyaBuilderHashChain.inc` | ~640 lines | Set+map chain algorithms | Mechanical helper extraction only |
| `ZmeyaBlobValidate.h` | ~500 lines | Validation + dispatch | Accept or split validators with mirrored tests |
| `ZmeyaBench.cpp` | ~620 lines | Many benchmarks | Optional: one TU per benchmark family |

## Recommended Refactor Plan

1. **Safe deletions:** Remove `ZMEYA_VALIDATE_HASH_DUPLICATES` macro block if you confirm no out-of-tree use; otherwise implement duplicate detection behind it in one place (insert/rehash).
2. **Low-risk simplifications:** Comment "navigation map" on serialize/build includes; test-only wrapper for stressed `write_scope`; guard-clause refactor inside `assign_string_std` with no behavior change.
3. **Medium-risk refactors:** Split `ZmeyaTestCoverage.cpp`; extract small static helpers from `ZmeyaBuilderHashChain.inc` with full test suite runs.
4. **Larger architectural cleanups:** Replacing TLS with an explicit context parameter on all mutators would shrink hidden state but is a breaking API tsunami; defer unless you accept a v2 write API.

## What Not To Change

- **TLS + `ScopedBuilder` for nested assigns:** Required so string/hash operations see the same `BuilderBase` when callers pass an explicit builder reference (`assign_string_std` uses `ScopedBuilder`).
- **`hm_g` / `hs_g` + `get_ptr_unsafe_to_store` rebind dance** after `alloc_aligned` in hash code: this is the core correctness contract for vector-backed arenas; "simplifying" it away without proof is unsafe.
- **`.inc` fragmentation:** Likely supports compile-time and separation of concerns; collapsing into a single mega-header would hurt readability in a different dimension.
- **`ZMEYA_ENABLE_SERIALIZE_SUPPORT` split:** Keep deserialize-only builds lean; do not merge serialize code behind `#ifdef` without a strong reason.

---

*Audit produced using repository scan (line counts via script), `rg` cross-references, and spot reads of hot headers. Re-run after large refactors.*

## Follow-up (post-audit)

The recommendations above were applied in-tree: `ZmeyaTestCoverage.cpp` was split into `ZmeyaTestCoverageCommon.h` plus `ZmeyaTestCoverage_P0P1.cpp` through `ZmeyaTestCoverage_DeathAndTail.cpp`; tests use `zmeya_test::write_scope_stressed` from `TestHelper.h`; `ZMEYA_VALIDATE_HASH_DUPLICATES` was removed; serialize/build `.inc` files gained short read-order maps; `assign_string_*` gained shared helpers in `ZmeyaSerializeApiAssignDetail.inc`; `AGENTS.md` / `NEXT_STEPS.md` and `is_stack_pointer` comments were aligned with behavior.

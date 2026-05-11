# Zmeya code complexity and maintainability audit

Senior code auditor review: readability, maintainability, cognitive load, simplicity, and unnecessary complexity. Assumes mid-level maintainers who need to reason quickly; prefers simple, direct code over clever abstractions.

---

## Executive Summary

Zmeya is a **header-only** library with a **clear read path** (trivial types, self-relative offsets) and a **write path** that is intentionally powerful but **hard to hold in your head**: thread-local active builder, many parallel entry points (`assign`, `BlobWriter::*`, member `push_back`/`insert` that all fan into the same internals), large `.inc` implementation bodies, and **two serialization strategies** for containers (incremental chained hash vs "snapshot to `std::unordered_*` and bulk rebuild"). That split is the main source of **branch explosion**, **near-duplication**, and **surprising performance** (non-incremental paths are O(n) per call).

**Top five fixes for maintainability (in order):**

1. **Document the write-path mental model in one place** (TLS scope, pointer invalidation, when incremental vs snapshot runs) and **fix README drift** ("no macros" is false; explain `ZMEYA_*` / `ZM_ASSIGN` honestly).
2. **Reduce API surface duplication**: pick a **primary** story (explicit `BlobWriter` + `assign(builder, …)` *or* TLS-only) and demote the other to "advanced" with narrow docs, or unify so mid-level readers see one pattern.
3. **Extract shared "STL snapshot + `impl_assign_*`"** for HashSet/HashMap insert/erase/clear fallbacks into one small set of helpers to kill copy-paste `if constexpr` blocks in `ZmeyaSerializeApi.inc`.
4. **Split `ZmeyaBuilderBase.inc`** into named files (arena/compaction, string, array, finalize/patch) even if still included -- engineers navigate by filename and search; one 700-line class body fights that.
5. **Delete or quarantine verified-unused helpers** (`ZM_ASSIGN`, `toAbsolute`, `zm::diff`) to shrink the "what is public?" confusion.

---

## Severity Legend

- **Critical:** Unsafe or blocks confident change (not many here from static review alone).
- **High:** Major cognitive load, multiple behaviors for one API, or doc that misleads.
- **Medium:** Duplication, oversized units, naming debt.
- **Low:** Polish, optional cleanups.

---

## Findings

### Finding: Write path has three visible "owners" of the same behavior (TLS, explicit builder, `BlobWriter` shims)

**Severity:** High

**Location:**

- File: `Zmeya/ZmeyaSerializeApi.inc`, `Zmeya/ZmeyaBlobWriter.inc`, `Zmeya/ZmeyaSerializeFoundation.h`
- Function/Class: `assign(...)`, `hashset_*` / `hashmap_*`, `Array::push_back`, `BlobWriter::array_push_back`, TLS (`get_global_builder`, `ScopedBuilder`)
- Lines: TLS in `ZmeyaSerializeFoundation.h` (~153-213); member forwards in `ZmeyaSerializeApi.inc` (~408-440); `BlobWriter` forwards (~482-523)

**Problem:** The same mutations are reachable via `zm::assign`, `zm::hashmap_insert(builder, …)`, `w.hashmap_insert(…)`, and `hm.insert()` (TLS). Each layer is thin, but the reader must know **which layer is "canonical"** and that **all depend on the same hidden TLS** for member methods.

**Why it matters:** Mid-level engineers will pick one style in one file and another elsewhere; reviews get noisy; bugs hide in "I used `assign` here but `insert` there" assumptions about reallocation and `ScopedBuilder` nesting.

**Recommendation:** In docs (single doc section): state **one recommended style** for app code (e.g. "prefer `w.*` inside `write_blob`; use `assign(builder,…)` only when TLS is wrong"). Optionally deprecate `ZM_ASSIGN` (see dead code). Long-term: consider **one** public mutation surface.

**Confidence:** High

---

### Finding: Container mutations split into incremental vs full resnapshot behind `if constexpr`

**Severity:** High

**Location:**

- File: `Zmeya/ZmeyaSerializeApi.inc`
- Function: `hashset_insert`, `hashset_erase`, `hashmap_insert`, `hashmap_erase`, etc.
- Lines: ~210-352 (representative)

**Problem:** Each function combines duplicate-key short-circuit, **fast path** (`hash*_chain_*`), and **slow path** (copy into `std::unordered_*` then `impl_assign_*`). HashSet and HashMap slow paths are structurally the same with small edits.

**Why it matters:** Changing one edge case requires touching several `if constexpr` branches; reviewers must reason about **key type** (`String` vs trivial), **F vs K**, and **incremental_ok** trait in one read. Performance characteristics change discontinuously by type -- easy to misuse in hot loops.

**Recommendation:** Extract **named** helpers: `hashset_snapshot_from_zm(...)`, `hashmap_snapshot_from_zm(...)`, `try_update_existing_key(...)`. Keep `if constexpr` at the **boundary** of those helpers, not repeated in four functions. Add a one-line comment at each public entry: **O(1) vs O(n)** when.

**Confidence:** High

---

### Finding: `impl_assign_hashset` and `impl_assign_hashmap` are near-duplicates

**Severity:** Medium

**Location:**

- File: `Zmeya/ZmeyaBuilderAssignImpl.inc`
- Function: `BuilderBase::impl_assign_hashset`, `BuilderBase::impl_assign_hashmap`
- Lines: ~81-158 and ~160-245

**Problem:** Same layout: note dead, empty fast path, bucket count, allocate buckets, initialize heads, allocate nodes, loop with hash, link chain, write header fields, register slots. Map adds value placement and extra re-fetch comments.

**Why it matters:** Any fix to bucket sizing, hash mix, or registration must be applied twice; drift risk.

**Recommendation:** Only merge if a shared helper **reads clearly** (e.g. template on node "write entry" lambda). If merging obscures Map's value lifetime, keep duplication but **extract identical preamble** (bucket count, alloc zeroed heads) into private `BuilderBase` methods.

**Confidence:** High

---

### Finding: `deep_copy` relies on `operator=` "doing the right thing"

**Severity:** Medium

**Location:**

- File: `Zmeya/ZmeyaSerializeApi.inc`
- Function: `deep_copy`
- Lines: ~151-169

**Problem:** Comment says assignment dispatches correctly; behavior is **non-local** (depends on `operator=` overloads elsewhere). That is powerful but **magic** when stepping in a debugger.

**Why it matters:** New types can silently pick the wrong `operator=`; hard to grep "all deep_copy behaviors".

**Recommendation:** Prefer **explicit** `deep_copy` overloads for public blob field types, or a single internal `dispatch_deep_copy(tag, …)` with visible cases. Keep `operator=` for ergonomics at the boundary only.

**Confidence:** Medium

---

### Finding: `BuilderBase` mixes arena compaction, patching, string growth, arrays, and hash dispatch in one class

**Severity:** Medium

**Location:**

- File: `Zmeya/ZmeyaBuilderBase.inc`
- Class: `detail::BuilderBase`
- Lines: roughly entire file (class closes ~701)

**Problem:** Single type owns unrelated subsystems; `compact_arena_and_remap_registry` alone is a long, stateful pipeline.

**Why it matters:** Hard to test pieces in isolation; blame/history noise; onboarding cost.

**Recommendation:** Physical split into includes (`BuilderArena.inc`, `BuilderStrings.inc`, …) **without** changing the class if you must -- same class, clearer files. Optionally PIMPL sub-structs for compaction vs mutation (bigger change).

**Confidence:** High

---

### Finding: Misleading / stale marketing vs code (README "No macros")

**Severity:** Medium

**Location:**

- File: `README.md`
- Lines: ~16

**Problem:** README claims **"No macros"**; the library uses **`ZMEYA_*` macros** extensively (`Zmeya/ZmeyaConfig.h`) and defines **`ZM_ASSIGN`** in `ZmeyaSerializeApi.inc`.

**Why it matters:** New engineers trust the README and then feel the codebase "lied"; they waste time reconciling mental model.

**Recommendation:** Replace with accurate wording: **"No IDL/codegen macros; small integration macros for asserts/allocators."** Remove or document `ZM_ASSIGN`.

**Confidence:** High

---

### Finding: `get_global_offset` has a TODO rename; name suggests "global" but means "offset in arena buffer"

**Severity:** Low

**Location:**

- File: `Zmeya/ZmeyaBuilderBase.inc`
- Function: `get_global_offset`
- Lines: ~360-367

**Problem:** `goffset_t` already means "global offset in blob"; `get_global_offset` overloads the word "global" with TLS "global builder" elsewhere.

**Why it matters:** Cross-reading TLS vs addressing vocabulary is unnecessarily ambiguous.

**Recommendation:** Rename to `arena_offset_of` / `byte_offset_in_arena` (mechanical rename with grep).

**Confidence:** Medium

---

### Finding: `goffset_t` is a type alias of `roffset_t`

**Severity:** Low

**Location:**

- File: `Zmeya/ZmeyaSerializeFoundation.h` (alias `using goffset_t = roffset_t;`)

**Problem:** Two names for identical type -- readers guess semantic difference.

**Why it matters:** Slight tax on every signature skim.

**Recommendation:** Use one name in new code, or a one-line comment at alias: **"same width; goffset_t is documentation for patch-time absolute byte index."**

**Confidence:** High

---

### Finding: Chained-hash code repeats "rebind pointer after alloc" (intentional but noisy)

**Severity:** Low (design tradeoff, well commented in places)

**Location:**

- File: `Zmeya/ZmeyaBuilderHashChain.inc`
- Example: `hashmap_chain_insert` re-fetches `p` many times
- Lines: ~293-359

**Problem:** Repetitive `p = reinterpret_cast<…>(get_ptr_unsafe_to_store(hm_g));` hurts linear reading.

**Why it matters:** Correct for reallocation safety; still **visual noise**.

**Recommendation:** Tiny inline `auto reload_hm(goffset_t g) -> HashMap*` in anonymous namespace in the `.inc` (or macro `ZMEYA_RELOAD` -- only if team accepts). Boring local lambda at function top is enough.

**Confidence:** Medium

---

## Dead Code Candidates

| Item | Location | Why unused | Safe to delete? | Verification |
|------|----------|------------|-----------------|----------------|
| `ZM_ASSIGN` macro | `Zmeya/ZmeyaSerializeApi.inc` (~548) | **No references** in repo except definition | **Probably safe** | Grep whole org / consumers if library is vendored widely |
| `zm::diff` | `Zmeya/ZmeyaSerializeFoundation.h` (~15-18) | **No call sites** in repo | **Probably safe** or keep if promised public API | Search consumers outside repo |
| `toAbsolute` | `Zmeya/ZmeyaTypes.h` (~22-26) | **Unused**; code uses `toAbsoluteAddr` | **Probably safe** | Same as above |
| `toAbsolute` vs `toAbsoluteAddr` duplication | `ZmeyaTypes.h` | Two one-liners same idea | If deleting `toAbsolute`, N/A | N/A |

**Not dead:** `write_blob_with_initial_buffer_bytes` -- heavily used in tests (`ZmeyaTestCoverage.cpp`, incremental tests, etc.); keep but keep **documented as test hook** (see `NEXT_STEPS.md` / `AGENTS.md`).

---

## Complexity Hotspots

| Hotspot | Why complex | Approx. source | Simplification |
|---------|-------------|----------------|----------------|
| `ZmeyaSerializeApi.inc` `hashset_*` / `hashmap_*` | `if constexpr` + duplicate containment checks + STL snapshot | ~200-400 lines region | Shared snapshot helpers; table in docs for O() |
| `BuilderBase::compact_arena_and_remap_registry` | Interval merge, remap registry, memcpy compaction | `ZmeyaBuilderBase.inc` ~125-213 | Extract phases with names; unit tests on remap invariants |
| `BuilderBase::string_append_cstr` | Growth policy, dead ranges, phys size map, re-fetch slots | `ZmeyaBuilderBase.inc` ~414-490 | Split "policy" (cap) vs "mechanics" (note dead, alloc, memcpy, register) |
| `BuilderBase::hashmap_chain_insert` | Init buckets, probe update, rehash, free list vs append | `ZmeyaBuilderHashChain.inc` ~293+ | Early returns already OK; consider "ensure_initialized" helper |

---

## Duplicate Patterns / Multiple Ways To Do The Same Thing

| Pattern A | Pattern B | Prefer | Action |
|-----------|-----------|--------|--------|
| `assign(zm_field, stl_value)` (TLS) | `assign(builder, zm_field, stl_value)` | **Explicit builder** for non-trivial code paths; **TLS** for small lambdas | Document; consider reducing surface later |
| `w.array_push_back` | `arr.push_back` (member) | **Either**; pick one in style guide | Same implementation -- OK if documented |
| Incremental `hash*_chain_*` | Snapshot + `impl_assign_*` | **Incremental** when `*_incremental_ok`; otherwise forced snapshot | Document performance cliff |
| `HashUtils::hashString` for `std::string` | `HashUtils::hasher` for generic | Keep both; **centralize** in `zm_hash_bucket_index_from_f` (already started) | Extend pattern to assign paths to avoid divergent hash |
| `ScopedBuilder` in some `assign_string_*` vs not in others | Mixed TLS restoration | **Consistent rule**: "any code that assigns into blob while temporarily switching builder must scope" | Audit call pairs; comment invariant at `assign_string_std` |

---

## Useless Abstractions / Wrappers

| Location | Wraps | Verdict |
|----------|-------|--------|
| `BlobWriter` methods that only forward to `impl_` or `zm::hash*` | `BuilderBase` | **Keep** -- stable user handle; not useless |
| `ZM_ASSIGN` | Cast + assign | **Delete** unless you commit to documenting it |
| `diff(offset_t,offset_t)` | subtraction | **Inline at sole future use or delete** |
| `toAbsolute` | same as `toAbsoluteAddr` with different typedef | **Delete one** or make one call the other |

---

## Large Function / Large File Review

| Location | Approx size | Mixed responsibilities | Split suggestion |
|----------|-------------|------------------------|------------------|
| `ZmeyaBuilderBase.inc` | ~700 lines, one class | Arena, dead compaction, strings, arrays, finalize, hash declarations | Multiple `.inc` files by topic |
| `ZmeyaSerializeApi.inc` | ~550 lines | `assign`, `deep_copy`, incremental hash API, `BlobWriter` bodies, macro | Split: `SerializeAssign.inc`, `SerializeDeepCopy.inc`, `SerializeIncremental.inc`, `SerializeBlobWriter.inc` |
| `ZmeyaBuilderHashChain.inc` | ~436 lines | HashSet/HashMap chain + rehash | Acceptable if kept focused; pair with shorter `SerializeApi` |

---

## Recommended Refactor Plan

1. **Safe deletions:** Remove `ZM_ASSIGN` if unused externally; remove `zm::diff` / `toAbsolute` after consumer grep; delete misleading README line.
2. **Low-risk simplifications:** README accuracy; rename `get_global_offset` (optional); extract snapshot helpers from `SerializeApi.inc`; file-split `.inc` only (no behavior change).
3. **Medium-risk refactors:** Consolidate HashSet/Map `impl_assign_*` shared preamble; narrow `deep_copy` magic with explicit dispatch.
4. **Larger cleanups:** Unify write API around explicit `BlobWriter` reference threading (removes TLS from mental model) -- **large** API break; only if worth it.

---

## What Not To Change

- **`.inc` inside `#include` from thin `.h` wrappers** -- ugly to some tastes, but it keeps **compile-time** boundaries and avoids circular include pain; not worth a revolution.
- **Re-fetching `p` after every `alloc_aligned`** in hash chain code -- repetitive but **documents a real hazard** (arena reallocation). Do not "optimize" away without a measured, safe abstraction.
- **`if constexpr` for slab-safe vs string-key paths** -- appropriate for C++17 header-only; the fix is **structure and docs**, not removing `constexpr`.
- **Thread-local active builder** -- controversial globally, but here it matches **`write_blob` session** semantics already documented; removing it is an architectural project, not a cleanup.

---

## Audit stance

The write path is **inherently** complex (self-relative memory, patching, optional arena compaction). The highest ROI is **shrinking the number of concepts a reader must track at once** (docs, dead code, file boundaries, deduplicating snapshot branches) -- not chasing theoretical purity in metaprogramming.

Prefer deletion over refactoring when code is unnecessary. Prefer inlining over helpers when the helper adds no meaning. Prefer explicit code over clever generic code.

# Builder API: design and migration plan

This document describes the **target** serialization/build UX for Zmeya (closure-based entry + TLS context), how it relates to **today's branch**, and a **phased plan** to reach that state.

---

## Target design (decisions)

### Closure-first API

The preferred user-facing shape is a **single entry point** that owns the build session and hides mechanics:

```cpp
zm::Span<char> blob = zm::build<MyRoot>([](MyRoot* root) {
    root->name = std::string("hello");
    root->items = std::vector<int>{1, 2, 3};
});
```

**Properties:**

- Root pointer behaves like a normal struct for assignments (`zm::` fields accept STL-shaped values via `operator=` / `zm::assign`).
- **`ScopedBuilder` (or equivalent) lives inside `zm::build`** — callers do not manually pair `Builder` + scope.
- **Finalize** runs inside the implementation after the lambda returns (exact ordering TBD: finalize-then-return Span vs return owning buffer).

Exact signatures (`initialSize`, error handling, `Span` lifetime) are implementation details to fix when adding `zm::build`.

### TLS as the active-builder context

**Assignments** (`zm::assign`, container `operator=`) resolve allocation against an **active builder** stored in **thread-local storage**.

**Why TLS:** Keeps assignment syntax natural (`root->field = ...`) without passing a context through every call.

**Threading rule (document everywhere):** Building must stay **single-threaded relative to zm mutations**. Work inside the closure **must not** spawn threads/task pools that assign into zm containers from **other** threads — child threads do not inherit TLS. Parallel CPU work is fine if results are merged **on the builder thread** before assigning into zm types.

Optional later: debug-only **owner-thread assertion** in `assign`.

### Why not a global mutex instead of TLS?

A single mutex around “the” builder serializes all builders across the process and hurts throughput. TLS matches **one active builder per thread** while preserving readable syntax.

---

## Documentation map

| Audience | File |
|----------|------|
| Quick overview + read path | `README.md` |
| Roadmap and migration | This file (`NEXT_STEPS.md`) |

---

## Current repo state vs desired state

### Snapshot (this branch)

| Area | State |
|------|--------|
| **BlobBuilder** | Implementation largely `#if 0` — legacy path retired |
| **Builder / BuilderBase** | Growing `std::vector<char>` buffer; **`zm::assign`** + container **`operator=`**; TLS via **`ScopedBuilder`** |
| **`zm::build`** | Not implemented yet — users still **manually** create `zm::Builder<Root>::create()` + **`ScopedBuilder`** |
| **Tests** | Many suites wrapped in **`#if 0`**; active coverage is mainly **`ZmeyaTest01`** + **`ZmeyaTestNewAPI`** |
| **Reallocation** | Documented issue: growth can invalidate offsets — **handle-based build** or other stable-addressing strategy still **planned** (see appendix) |
| **README** | Previously referenced **`BlobBuilder`** — updated to describe the new direction |

### Desired state

| Area | Target |
|------|--------|
| **Primary API** | **`zm::build`** (closure); **`ScopedBuilder`/`TLS`** internal detail |
| **README / examples** | Show **`zm::build`**; mention **`ZMEYA_ENABLE_SERIALIZE_SUPPORT`**; threading rule |
| **Tests** | Suites re-enabled incrementally; **`gtest_filter`** aligned with real test names |
| **Memory** | Reallocation addressed (appendix), **`referTo`-style sharing** decided (implement or drop) |
| **Dead code** | Remove or relocate **`#if 0`** graveyards once superseded |

---

## Migration plan (ordered phases)

### Phase 1 — Closure API (UX)

1. Add **`zm::build<TRoot>(Fn&&)`** (and overloads as needed: initial reserve size, alignment).
2. Implement by **constructing `Builder<TRoot>`**, installing **`ScopedBuilder`**, invoking lambda with **`getRoot()`**, then **`finalize()`** and returning **`Span<char>`** (or owning blob type if lifetime requires it).
3. Migrate **`ZmeyaTestNewAPI`** / **`ZmeyaTest01`** to **`zm::build`** as the primary pattern.
4. Document **`ScopedBuilder` + manual `Builder`** as **advanced / legacy** (still useful for non-closure control flow).

**Exit criteria:** Examples and tests prefer **`zm::build`**; no duplicate TLS setup in user code.

### Phase 2 — Tests and narrative cleanup

1. For each file **`ZmeyaTest02` … `ZmeyaTest11`**: remove file-level **`#if 0`**, port tests to **`Builder<>` + `zm::build`** (or keep explicit **`ScopedBuilder`** only where necessary).
2. Fix **`allocate` / root patterns** where old tests assumed **`BlobBuilder`** APIs.
3. Refresh **`NEXT_STEPS`** testing section with **commands that match real test case names**.
4. Align **`README`** “see unit tests” with actually enabled suites.

**Exit criteria:** CI runs a **meaningful** suite; doc claims match reality.

### Phase 3 — Stable addressing (reallocation)

Problem: **`std::vector<char>`** growth can move storage and break relative offsets computed earlier.

Direction (pick one or combine):

- **Handle-based building + finalize** (convert handles to offsets once), as sketched previously in this repo; or
- **Reserved capacity heuristics** + documented worst-case **two-pass** build; or
- **Chunked/slab allocator** with stable ids — heavier change.

**Exit criteria:** Stress tests (**large arrays/strings**) pass; **`contains_pointer`** failures gone under growth.

### Phase 4 — Polish and API debt

1. **`referTo` / sharing** — reintroduce semantics compatible with final blob layout, or document omission.
2. **`BlobPtr`** / **`getRawByAbsoluteOffset`** — align or remove dead surfaces so headers compile cleanly for all intended uses.
3. **`alloc_algined` typo**, **`#if 0`** blocks inside **`assign`**, portable **`is_stack_pointer`** if non-Windows matters.
4. Optional: **`BuildSession::assign`** overloads for advanced MT-safe paths (explicit context) — only if needed.

---

## Appendix: handle-based building (technical sketch)

The builder buffer may **reallocate**. Offsets stored into zm containers during growth can become invalid if they pointed into the buffer using **absolute** identity of earlier pointers.

A **two-phase** approach remains the leading fix: during construction, store **stable handles** (or stable allocation ids); during **finalize**, emit one contiguous blob and rewrite storage into **self-relative offsets** only.

Details (allocation records, flag bits in **roffset_t**, walking structures at finalize) belong in design notes when Phase 3 starts; they are intentionally not duplicated in full here so this file stays maintainable.

---

## Appendix: old test filter commands

Replace placeholder filters after Phase 2 with names from:

```bash
ZmeyaTest.exe --gtest_list_tests
```

---

## History note

Earlier revisions of this file mixed **target API samples** (`zm::Builder::create()`, `allocate_root`) that did not match the implementation (`zm::Builder<Root>::create()`, **`getRoot()`** after constructor allocates root). **Closure + TLS** supersedes those samples; **`zm::build`** is the documented entry going forward.

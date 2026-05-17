# Blob writer notes and roadmap

## Current API

- **`zm::write_scope<TRoot>(fn)`** returns an owning **`std::vector<char>`** after calling **`fn(w)`** with **`zm::BlobWriter<TRoot>`** (root + **`allocate`**, **`contains_pointer`**, **`builder_base()`**, etc.). Implementation types live in **`zm::detail`** only.

- **`zm::assign(zm::detail::BuilderBase& builder, ...)`** overloads install the builder as the active context for **`assign`** / **`deep_copy`** without relying on **`write_scope`** alone (TLS is still used inside **`operator=`** on **`zm::*`** types for ergonomics).

- Relative-offset slots (**`Pointer`**, **`Array`** headers, **`String::data`**, nested **`HashMap`/`HashSet`** **`Array`** headers) are recorded on **`detail::BuilderBase`** with parallel **`goffset_t`** targets; **`finalize`** runs a patch pass so sealed bytes stay consistent with that metadata.

## Threading

Do not assign into **`zm::*`** containers from worker threads during **`zm::write_scope`**; TLS is per-thread and child threads do not inherit it.

## Blob growth (reallocation)

The backing store is **`std::vector<char>`**, which can reallocate when it grows.

**Indices into the arena (`goffset_t`) stay valid across realloc**; self-relative **`roffset_t`** fields written correctly while both sides live in the arena remain coherent when the whole buffer moves together.

**`zm::ArenaRef<T>`** from **`w.root()`** and **`w.allocate<T>()`** re-resolves through the live arena on each **`operator->` / `operator*`** use, so field access stays valid across realloc. Call **`transient_ptr()`** only when you need a raw **`T*`** for **`zm::Pointer`**, **`std::vector<T*>`**, **`contains_pointer`**, or C-style helpers; do not store that raw pointer across growth.

Some unit tests call **`zmeya_test::write_scope_stressed`** (see **`TestHelper.h`**) with a tiny starting arena to force **`std::vector`** reallocations on purpose; it forwards to **`zm::detail::write_scope_with_initial_buffer_bytes`** and is not a supported public workflow for application code.

## Incremental mutation

**`Array`**, **`String`**, **`HashSet`**, and **`HashMap`** support incremental APIs during **`write_scope`** (see **`AGENTS.md`**). **`array_push_back`** leaves prior slabs in the arena (bump-only **Q2**); sealed blobs may be **larger** than a minimal bulk **`assign`** for the same logical content.

## Appendix: tests

Run **`ZmeyaTest.exe --gtest_list_tests`** to refresh filters after adding or renaming suites.

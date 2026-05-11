# Blob writer notes and roadmap

## Current API

- **`zm::write_blob<TRoot>(fn)`** returns an owning **`std::vector<char>`** after calling **`fn(w)`** with **`zm::BlobWriter<TRoot>`** (root + **`allocate`**, **`contains_pointer`**, **`builder_base()`**, etc.). Implementation types live in **`zm::detail`** only.

- **`zm::assign(zm::detail::BuilderBase& builder, ...)`** overloads install the builder as the active context for **`assign`** / **`deep_copy`** without relying on **`write_blob`** alone (TLS is still used inside **`operator=`** on **`zm::*`** types for ergonomics).

- Relative-offset slots (**`Pointer`**, **`Array`** headers, **`String::data`**, nested **`HashMap`/`HashSet`** **`Array`** headers) are recorded on **`detail::BuilderBase`** with parallel **`goffset_t`** targets; **`finalize`** runs a patch pass so sealed bytes stay consistent with that metadata.

## Threading

Do not assign into **`zm::*`** containers from worker threads during **`zm::write_blob`**; TLS is per-thread and child threads do not inherit it.

## Blob growth (reallocation)

The backing store is **`std::vector<char>`**, which can reallocate when it grows.

**Indices into the arena (`goffset_t`) stay valid across realloc**; self-relative **`roffset_t`** fields written correctly while both sides live in the arena remain coherent when the whole buffer moves together.

**Raw pointers** returned by **`allocate()`**, **`root()`**, addresses of **`zm::*`** objects in the blob, or pointers returned by **`get()`** can become **stale** after a realloc if you cache them across operations that grow the buffer. Refresh them after growth (e.g. call **`writer.root()`** again before touching nested fields, or keep **`goffset_t`** instead of **`T*`** until finalize).

Stress tests use a **large initial reserve** when capturing many raw pointers across many **`assign`** steps so the implementation stays simple and deterministic.

## Appendix: tests

Run **`ZmeyaTest.exe --gtest_list_tests`** to refresh filters after adding or renaming suites.

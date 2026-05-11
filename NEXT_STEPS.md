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

**Raw pointers** returned by **`allocate()`**, **`root()`**, addresses of **`zm::*`** objects in the blob, or pointers returned by **`get()`** can become **stale** after a realloc if you cache them across operations that grow the buffer. Call **`writer.root()`** again after growth, resolve through **`writer.builder_base()`** using **`goffset_t`** plus **`get_ptr_unsafe_to_store`**, or otherwise avoid holding **`T*`** across allocator growth.

Some unit tests call **`zm::detail::write_blob_with_initial_buffer_bytes`** with a tiny starting arena to force **`std::vector`** reallocations on purpose; that helper is not a supported public workflow for application code.

## Incremental mutation

**`Array`**, **`String`**, **`HashSet`**, and **`HashMap`** support incremental APIs during **`write_blob`** (see **`AGENTS.md`**). **`array_push_back`** leaves prior slabs in the arena (bump-only **Q2**); sealed blobs may be **larger** than a minimal bulk **`assign`** for the same logical content.

## Appendix: tests

Run **`ZmeyaTest.exe --gtest_list_tests`** to refresh filters after adding or renaming suites.

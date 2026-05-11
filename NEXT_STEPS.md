# Blob writer notes and roadmap

## Current API

- **`zm::write_blob<TRoot>(fn)`** returns an owning **`std::vector<char>`** after calling **`fn(w)`** with **`zm::BlobWriter<TRoot>`** (root + **`allocate`**, etc.). Implementation types live in **`zm::detail`** only.

Assignments into **`zm::*`** fields use each type's **`operator=`** (STL-shaped RHS and **`zm::Pointer<T> = T*`**), resolved against the **thread-local** active blob writer installed for that **`zm::write_blob`** call.

## Threading

Do not assign into **`zm::*`** containers from worker threads during **`zm::write_blob`**; TLS is per-thread and child threads do not inherit it.

## Blob growth (reallocation)

The internal backing store uses **`std::vector<char>`**, which can **reallocate** when it grows. Relative offsets stored in **`zm::*`** fields can become invalid if the buffer moves mid-write; stress tests pass a **large initial reserve** as the second argument to **`zm::write_blob`** so the heap block stays stable for that session.

Possible directions when this becomes a priority:

- Handle-based construction with a single finalize pass that emits final relative offsets; or
- Reserve capacity heuristics plus documented **two-pass** write; or
- Chunked / slab backing storage with stable logical addresses.

## Appendix: tests

Run **`ZmeyaTest.exe --gtest_list_tests`** to refresh filters after adding or renaming suites.

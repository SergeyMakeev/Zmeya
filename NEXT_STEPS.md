# Builder API notes and roadmap

## Current API

- **`zm::build<TRoot>(fn)`** returns an owning **`std::vector<char>`** after calling **`fn(session)`** with **`zm::BuildSession<TRoot>`** (root + **`allocate`**, etc.). Builder implementation types live in **`zm::detail`** only.

Assignments into **`zm::*`** fields use each type's **`operator=`** (STL-shaped RHS and **`zm::Pointer<T> = T*`**), resolved against the **thread-local** active builder installed for that **`zm::build`** call.

## Threading

Do not assign into **`zm::*`** containers from worker threads during a build; TLS is per-thread and child threads do not inherit it.

## Builder growth (reallocation)

The internal builder stores bytes in **`std::vector<char>`**, which can **reallocate** when it grows. Relative offsets stored in **`zm::*`** fields can become invalid if the buffer moves mid-build; stress tests pass a **large initial reserve** as the second argument to **`zm::build`** so the heap block stays stable for that session.

Possible directions when this becomes a priority:

- Handle-based construction with a single finalize pass that emits final relative offsets; or
- Reserve capacity heuristics plus documented **two-pass** build; or
- Chunked / slab backing storage with stable logical addresses.

## Appendix: tests

Run **`ZmeyaTest.exe --gtest_list_tests`** to refresh filters after adding or renaming suites.

# Builder API notes and roadmap

## Current API

- **`zm::build<TRoot>(fn)`** returns an owning **`std::vector<char>`**. **`ScopedBuilder`** installs the TLS active builder for the closure (see **`Zmeya/Zmeya.h`**).
- **`zm::Builder<TRoot>::create()`** plus **`ScopedBuilder`** and **`finalize()`** cover non-linear control flow (see tests).

Assignments into **`zm::*`** fields use **`zm::assign`** and container **`operator=`**, resolved against the **thread-local** active builder.

## Threading

Do not assign into **`zm::*`** containers from worker threads during a build; TLS is per-thread and child threads do not inherit it.

## Builder growth (reallocation)

**`BuilderBase`** stores bytes in **`std::vector<char>`**, which can **reallocate** when it grows. Relative offsets stored in **`zm::*`** fields can become invalid if the buffer moves mid-build; stress tests often pass a **large initial reserve** to **`Builder<>::create(n)`** so the heap block stays stable for that session.

Possible directions when this becomes a priority:

- Handle-based construction with a single finalize pass that emits final relative offsets; or
- Reserve capacity heuristics plus documented **two-pass** build; or
- Chunked / slab backing storage with stable logical addresses.

## Appendix: tests

Run **`ZmeyaTest.exe --gtest_list_tests`** to refresh filters after adding or renaming suites.

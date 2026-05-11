# Zmeya repository guide (build, test, layout)

This file is for humans and coding agents so the next session does not rediscover CMake paths and flags by trial and error.

## Layout

| Path | Role |
|------|------|
| `Zmeya/Zmeya.h` | Header-only library (deserialize always; **`zm::write_blob`** needs `ZMEYA_ENABLE_SERIALIZE_SUPPORT`) |
| `Zmeya/CMakeLists.txt` | INTERFACE target **`Zmeya`** (include dir + C++17) |
| Root `CMakeLists.txt` | Executable **`ZmeyaTest`** (all **`ZmeyaTest*.cpp`** including **`ZmeyaTestIncremental.cpp`**, plus **`TestHelper`**) |
| `extern/googletest` | GoogleTest / gtest_main (pulled as submodule or vendored per your checkout) |

Serialization-related tests require **`ZMEYA_ENABLE_SERIALIZE_SUPPORT`**. The root CMakeLists adds it globally for **`ZmeyaTest`** (`add_definitions(-DZMEYA_ENABLE_SERIALIZE_SUPPORT)`).

## Incremental write APIs (serialize builds)

With **`ZMEYA_ENABLE_SERIALIZE_SUPPORT`**, during **`zm::write_blob`** you can mutate **`zm::Array`**, **`zm::String`**, **`zm::HashSet`**, and **`zm::HashMap`** incrementally.

### Write path: TLS, pointers, and which API to use

- **`zm::write_blob`** installs a **`detail::BuilderBase`** in **thread-local storage** for the duration of your lambda. **`zm::`** member mutators (**`arr.push_back`**, **`hm.insert`**, **`operator=`** on fields) read that TLS context.
- **Raw pointers** into the arena (from **`w.root()`**, **`get()`**, **`allocate`**, etc.) can go stale after any step that **grows** the backing **`std::vector<char>`**; re-derive from **`goffset_t`** / **`w.root()`** / **`builder_base()`** after growth.
- **Recommended style in app code:** prefer **`zm::BlobWriter<Root>::`** methods (**`w.hashmap_insert(...)`**, **`w.array_push_back(...)`**, **`assign` via `=`** on **`w.root()`**) so the active builder is obvious at the call site. Use **`zm::assign(builder, ...)`** / **`zm::hashmap_insert(builder, ...)`** when you must target an **explicit** **`BuilderBase&`** (nested **`ScopedBuilder`**, tests, or code that cannot rely on TLS alone).
- **Incremental vs snapshot:** when **`zm_hash*_chain_incremental_ok`** is true for the key/value types, hash edits are **O(1)** amortized on the chain tables. Otherwise each **`insert`/`erase`/`clear`** **rebuilds** from an **`std::unordered_*` snapshot** (**O(n)** per call). Prefer bulk **`assign(unordered_map, ...)`** for large batches on those types.

- **`BlobWriter`**: **`array_push_back`**, **`array_pop_back`**, **`array_clear`**, **`array_erase_at`**, **`array_resize`**, **`string_append`**, **`string_clear`**, **`hashset_insert`**, **`hashset_erase`**, **`hashset_clear`**, **`hashmap_insert`**, **`hashmap_erase`**, **`hashmap_clear`**
- **Member helpers** (same session, TLS from **`write_blob`**): **`Array::push_back`**, **`String::append`** / **`operator+=`**, **`HashSet::insert`** / **`erase`** / **`clear`**, **`HashMap::insert`** / **`erase`** / **`clear`**
- **Explicit builder**: **`zm::assign(detail::BuilderBase&, ...)`**, **`zm::hashset_insert(builder, ...)`**, **`zm::hashmap_insert(builder, ...)`**, etc.

**`assign_string_std`** / **`assign_string_cstr`** wrap the destination slot with **`ScopedBuilder`** so nested writes see the same builder even when the caller passed an explicit **`BuilderBase&`**.
**`array_push_back`** is only supported for **slab-memcpy-safe** element types (see **`detail::zm_array_push_back_ok`** in **`Zmeya.h`**). It excludes **`zm::String`**, **`zm::Pointer`**, nested **`zm::Array`**, and similar edge-bearing types; use **`assign(std::vector<...>)`** for those.

Incremental growth uses a **bump allocator**; abandoned slabs remain in the arena, so the final **`std::vector<char>`** can be **larger** than a minimal one-shot assign for the same logical content. Prefer comparing **logical** fields after **`finalize`**, not only raw **`memcmp`** of the whole blob.

## Prerequisites

- **CMake** 2.8.12 or newer (3.x recommended).
- **C++17** toolchain:
  - **Windows:** Visual Studio with the **Desktop development with C++** workload and a matching **Windows SDK** (CMake `-A x64` matches most docs in this repo).
  - **Linux/macOS:** GCC or Clang with C++17.

If CMake errors about **generator platform** vs an old cache, delete the build directory or remove `CMakeCache.txt` and reconfigure.

If MSVC reports a missing **Windows SDK** version, install that SDK or retarget the solution to an installed SDK version.

## Configure and build

Always configure from the **repository root** (where the root `CMakeLists.txt` lives).

### Visual Studio generator (Windows, x64 example)

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Output binary:

- `build\Release\ZmeyaTest.exe` (or `build\Debug\ZmeyaTest.exe` for Debug).

Run from `build\Release` (or add to PATH):

```bat
ZmeyaTest.exe
ZmeyaTest.exe --gtest_list_tests
ZmeyaTest.exe --gtest_filter=ZmeyaTestSuite.MMapTest
```

### Single-configuration generators (Ninja / Make)

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Output (typical):

- `build/ZmeyaTest`

```sh
cd build && ctest --output-on-failure
./ZmeyaTest --gtest_filter=ZmeyaTestSuite.*
```

## Tests and temporary files

Some tests write files next to the **current working directory** (e.g. `test.zm`, `mmaptest.zm`). Run **`ZmeyaTest`** from a writable directory if needed.

`ZmeyaTest10` uses memory-mapped files on **Windows**; on other platforms it reads the file into a buffer and validates the same layout.

### Windows build scripts

Root **`build_debug.cmd`** configures CMake, builds **Debug** `ZmeyaTest`, then runs **`OpenCppCoverage.exe`** when that tool is on `PATH`. If OpenCppCoverage is not installed, it runs **`build\Debug\ZmeyaTest.exe`** directly so the script still validates tests.

**`build_release.cmd`** does the same flow for **Release** (`cmake --build` with `--config Release`, tests from **`build\Release\ZmeyaTest.exe`**).

### Blob writer growth and raw pointers

Internally, the blob buffer uses `std::vector<char>` and can **reallocate** when it grows. Treat any raw pointer into the arena (including from **`w.allocate`**, **`w.root()`**, **`get()`**, or element pointers) as **invalid after a growth step** unless you re-derive it from a **`goffset_t`** or call **`w.root()`** / **`builder_base()`** again. **`detail::BuilderBase::arena_byte_offset_of(ptr)`** returns the byte index of **`ptr`** in the arena (for tests and low-level code). **`zm::write_blob`** pre-reserves a fixed default arena size; it does **not** expose a user-controlled initial capacity.

The builder records self-relative **slot** targets in an **`std::unordered_map<goffset_t, goffset_t>`**; **`finalize`** pads to alignment then **patches** every registered word. **`assign` from empty** STL containers or empty strings **clears** the destination **`zm::`** field when it was previously non-empty.

### Debug vs Release

**`ZmeyaTest04` (`ListTest`)** uses many more nodes in Release than in Debug; the test links nodes using **`goffset_t`** so it does not cache stale **`T*`** across reallocations.

## Documentation

| File | Purpose |
|------|---------|
| `README.md` | Library overview and usage |
| `NEXT_STEPS.md` | **`write_blob`**, TLS, reallocation, incremental APIs, registry / finalize |

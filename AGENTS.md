# Zmeya repository guide (build, test, layout)

This file is for humans and coding agents so the next session does not rediscover CMake paths and flags by trial and error.

## Layout

| Path | Role |
|------|------|
| `Zmeya/Zmeya.h` | Header-only library (deserialize always; **`zm::write_scope`** needs `ZMEYA_ENABLE_SERIALIZE_SUPPORT`) |
| `Zmeya/CMakeLists.txt` | INTERFACE target **`Zmeya`** (include dir + C++17) |
| Root `CMakeLists.txt` | Executable **`ZmeyaTest`** (all **`ZmeyaTest*.cpp`**, **`ZmeyaTestCoverage_*.cpp`**, **`ZmeyaTestIncremental.cpp`**, plus **`TestHelper`**) |
| `ZmeyaBench.cpp` | Optional **`ZmeyaBench`** executable (Google Benchmark microbenchmarks; enable with **`ZMEYA_BUILD_BENCHMARKS`**) |
| `ZmeyaBenchSupport.cpp` | **`zm::onAssertionFailed`** stub for **`ZmeyaBench`** (tests implement assertions elsewhere; see **`ZmeyaConfig.h`**) |
| `extern/googletest` | GoogleTest / gtest_main (pulled as submodule or vendored per your checkout) |

Serialization-related tests require **`ZMEYA_ENABLE_SERIALIZE_SUPPORT`**. The root CMakeLists adds it globally for **`ZmeyaTest`** (`add_definitions(-DZMEYA_ENABLE_SERIALIZE_SUPPORT)`).

## Incremental write APIs (serialize builds)

With **`ZMEYA_ENABLE_SERIALIZE_SUPPORT`**, during **`zm::write_scope`** you can mutate **`zm::Array`**, **`zm::String`**, **`zm::HashSet`**, and **`zm::HashMap`** incrementally.

### Write path: TLS, pointers, and which API to use

- **`zm::write_scope`** installs a **`detail::BuilderBase`** in **thread-local storage** for the duration of your lambda. **`zm::`** member mutators (**`arr.push_back`**, **`hm.insert`**, **`operator=`** on fields) read that TLS context. **`require_tls_builder`** (default **`zm::assign`** / member mutators) asserts when TLS has no builder (wrong thread or outside **`write_scope`**). In Debug, or when you define **`ZMEYA_DEBUG_TLS_BUILDER_THREAD`**, it also asserts that the installer thread id matches the current thread (defense in depth).
- **`zm::ArenaRef<T>`** from **`w.root()`** and **`w.allocate<T>()`** holds a **byte offset** into the arena plus the active builder; **`root->field`** re-resolves on each access. A raw **`T*`** from **`transient_ptr()`** (or from **`get()`** on read views) can still go stale after growth if you **cache** it across allocator steps; use **`goffset_t`** / **`builder_base()`** when you need explicit low-level recovery.
- **Recommended style in app code:** prefer **`zm::BlobWriter<Root>::`** methods (**`w.hashmap_insert(...)`**, **`w.array_push_back(...)`**, **`assign` via `=`** on **`w.root()`**) so the active builder is obvious at the call site. Use **`zm::assign(builder, ...)`** / **`zm::hashmap_insert(builder, ...)`** when you must target an **explicit** **`BuilderBase&`** (nested **`ScopedBuilder`**, tests, or code that cannot rely on TLS alone). The free **`zm::hashmap_*`** / **`zm::hashset_*`** / **`zm::assign`** entry points hold the real implementations; **`BlobWriter`** methods are thin forwards for readability at the call site.
- **Incremental hash:** **`insert`/`erase`/`clear`** on **`zm::HashMap`** / **`zm::HashSet`** use the chain tables (**amortized O(1)** per op plus occasional **O(n)** rehash when **`live_count >= bucket_count`**). Node storage grows via **`BuilderBase::hash_chain_nodes_array_grow_append_default_*`** in **`ZmeyaBuilderHashChainNodesGrow.inc`** (slab reallocate and per-slot string copy via **`assign_string_std`**, not a full-table **`std::unordered_*`** snapshot). Bulk **`assign(unordered_map, ...)`** remains the right tool when you already have a complete STL map to load at once.

- **`BlobWriter`**: **`array_push_back`**, **`array_pop_back`**, **`array_clear`**, **`array_erase_at`**, **`array_resize`**, **`string_append`**, **`string_clear`**, **`hashset_insert`**, **`hashset_erase`**, **`hashset_clear`**, **`hashset_reserve_nodes`**, **`hashmap_insert`**, **`hashmap_erase`**, **`hashmap_clear`**, **`hashmap_reserve_nodes`**
- **Member helpers** (same session, TLS from **`write_scope`**): **`Array::push_back`**, **`String::append`** / **`operator+=`**, **`HashSet::insert`** / **`erase`** / **`clear`**, **`HashMap::insert`** / **`erase`** / **`clear`**
- **Explicit builder**: **`zm::assign(detail::BuilderBase&, ...)`**, **`zm::hashset_insert(builder, ...)`**, **`zm::hashmap_insert(builder, ...)`**, **`zm::hashset_reserve_nodes(builder, ...)`**, **`zm::hashmap_reserve_nodes(builder, ...)`**, etc.

**`assign_string_std`** / **`assign_string_cstr`** wrap the destination slot with **`ScopedBuilder`** so nested writes see the same builder even when the caller passed an explicit **`BuilderBase&`**.
**`array_push_back`** is only supported for **slab-memcpy-safe** element types (see **`detail::zm_array_push_back_ok`** in **`Zmeya.h`**). It excludes **`zm::String`**, **`zm::Pointer`**, nested **`zm::Array`**, and similar edge-bearing types; use **`assign(std::vector<...>)`** for those.

Incremental growth uses a **bump allocator**; abandoned slabs remain in the arena, so the final **`std::vector<char>`** can be **larger** than a minimal one-shot assign for the same logical content. Prefer comparing **logical** fields after **`finalize`**, not only raw **`memcmp`** of the whole blob.

## Prerequisites

- **CMake** 3.14 or newer (required for optional Google Benchmark via **`FetchContent`**; 3.x recommended otherwise).
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

Optional microbenchmarks (**Google Benchmark** is fetched at configure time when **`ZMEYA_BUILD_BENCHMARKS=ON`**; first configure needs network access):

```bat
cmake -S . -B build -DZMEYA_BUILD_BENCHMARKS=ON -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target ZmeyaBench
```

Output:

- `build\Release\ZmeyaBench.exe`

Example:

```bat
ZmeyaBench.exe --benchmark_out=zmeya_bench.json --benchmark_out_format=json
ZmeyaBench.exe --benchmark_filter=BM_HashMapInt32_
```

Use **Release** builds when interpreting timings. **`items_per_second`** counters reflect logical elements processed per **`write_scope`** iteration where applicable.

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
- `build/ZmeyaBench` if configured with **`-DZMEYA_BUILD_BENCHMARKS=ON`** and built with **`cmake --build build --target ZmeyaBench`**

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

Internally, the blob buffer uses `std::vector<char>` and can **reallocate** when it grows. **`zm::ArenaRef<T>`** from **`w.root()`** / **`w.allocate<T>()`** stays valid across realloc for **`operator->` / `operator*`**; a raw **`T*`** from **`transient_ptr()`** (or **`get()`** on read views) is **invalid after a growth step** if you cached it. Re-derive from **`goffset_t`** and **`builder_base()->get_ptr_unsafe_to_store`**, or call **`w.root()`** / **`allocate`** again. **`detail::BuilderBase::arena_byte_offset_of(ptr)`** returns the byte index of **`ptr`** in the arena (for tests and low-level code). **`zm::write_scope`** pre-reserves a fixed default arena size; it does **not** expose a user-controlled initial capacity.

Tests that must force reallocations use **`zmeya_test::write_scope_stressed`** in **`TestHelper.h`**, which forwards to **`zm::detail::write_scope_with_initial_buffer_bytes`** with a caller-chosen starting **`reserve`**. That path is for tests only, not application code.

**`get_relative_offset`** rejects pointers that **`is_stack_pointer`** classifies as stack addresses (**Windows**: `GetCurrentThreadStackLimits`; **macOS**: `pthread_get_stackaddr_np` / `pthread_get_stacksize_np`; **Linux glibc**: `pthread_getattr_np` when **`_GNU_SOURCE`** is in effect, which the **`Zmeya`** CMake target defines on Linux). Other environments may still return false here, so treat the guard as best-effort outside those paths.

The builder records self-relative **slot** targets in an **`std::unordered_map<goffset_t, goffset_t>`**; **`finalize`** pads to alignment then **patches** every registered word. **`assign` from empty** STL containers or empty strings **clears** the destination **`zm::`** field when it was previously non-empty.

### Debug vs Release

**`ZmeyaTest04` (`ListTest`)** uses many more nodes in Release than in Debug; the test links nodes using **`goffset_t`** so it does not cache stale **`T*`** across reallocations.

## Documentation

| File | Purpose |
|------|---------|
| `README.md` | Library overview and usage |
| `NEXT_STEPS.md` | **`write_scope`**, TLS, reallocation, incremental APIs, registry / finalize |

**Blob validation:** composite `TRoot` structs are only shallow-checked unless you specialize **`zm::blob_root_deep_validate<TRoot>`** (`enabled = true` and `validate` calling **`BlobLayoutValidator::field_dispatch`** on each field) or use a direct **`zm::`** container as the root. Use **`validate_blob_view_strict` / `as_root_blob_strict`** when you want a compile-time guard that deep validation exists. **`try_c_str_in_blob`** and **`zm::detail::self_rel_target_address`** support bounded string reads and checked self-relative math.

**Sanitizers:** configure with **`cmake -DZMEYA_ENABLE_ASAN=ON`** (non-MSVC) to build **`ZmeyaTest`** with AddressSanitizer.

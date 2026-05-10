# Zmeya repository guide (build, test, layout)

This file is for humans and coding agents so the next session does not rediscover CMake paths and flags by trial and error.

## Layout

| Path | Role |
|------|------|
| `Zmeya/Zmeya.h` | Header-only library (deserialize always; serialize/build APIs need `ZMEYA_ENABLE_SERIALIZE_SUPPORT`) |
| `Zmeya/CMakeLists.txt` | INTERFACE target **`Zmeya`** (include dir + C++17) |
| Root `CMakeLists.txt` | Executable **`ZmeyaTest`** (all `ZmeyaTest*.cpp` + `TestHelper`) |
| `extern/googletest` | GoogleTest / gtest_main (pulled as submodule or vendored per your checkout) |

Serialization-related tests require **`ZMEYA_ENABLE_SERIALIZE_SUPPORT`**. The root CMakeLists adds it globally for **`ZmeyaTest`** (`add_definitions(-DZMEYA_ENABLE_SERIALIZE_SUPPORT)`).

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

### `build.cmd` (Windows)

Root **`build.cmd`** configures CMake, builds **Debug** `ZmeyaTest`, then runs **`OpenCppCoverage.exe`** when that tool is on `PATH`. If OpenCppCoverage is not installed, it runs **`build\Debug\ZmeyaTest.exe`** directly so the script still validates tests.

### Builder growth and raw pointers

`BuilderBase` stores data in a `std::vector<char>` that can **reallocate** when it grows. Raw pointers returned by **`allocate<T>()`** or captured before a large **`assign`** are only valid while the buffer does not move. Stress tests pass a **large initial size** to **`Builder<>::create(n)`** so the buffer stays stable for that session. A future handle-based or slab builder would remove this constraint.

### Debug vs Release

**`ZmeyaTest04` (`ListTest`)** uses many more nodes in Release than in Debug; it reserves a larger builder buffer in Release so pointer assignments stay valid.

## Documentation

| File | Purpose |
|------|---------|
| `README.md` | Library overview and usage |
| `NEXT_STEPS.md` | Builder API roadmap and migration notes |

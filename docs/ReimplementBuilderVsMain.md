# Zmeya before and after: user-focused comparison (`main` vs `reimplement_builder`)

This document compares **`main`** and **`reimplement_builder`** the way an **application author** experiences them: day-to-day tasks, typical code shapes, and what changes when you evolve your **root struct** and **compound types**.

Technical layout, CMake, and on-disk hash encoding are summarized briefly at the end for migration planning.

---

## 1. Mental model (what did not change)

**Read path (mmap, network buffer, heap block):** you still treat Zmeya data as **one contiguous byte range** and use **`const Root*`** (or **`const zm::String&`**, etc.) with **no parse step**. Self-relative **`zm::Pointer`**, **`zm::String`**, **`zm::Array`**, **`zm::HashMap`**, **`zm::HashSet`** behave the same *idea*: offsets from field addresses, not raw process pointers.

**What changed most for authors:** how you **build** a blob (session API, **`zm::ArenaRef`**, spelling of "copy STL into zm fields", optional **incremental** mutation), how you **hold** finalized bytes, and **hash table** wire layout (see appendix).

---

## 2. User story: "I only consume blobs"

| | `main` | `reimplement_builder` |
|---|--------|------------------------|
| Include | `#include "Zmeya.h"` (single file at repo root in this project) | `#include "Zmeya.h"` via the **`Zmeya`** CMake target include paths (**`Zmeya/Zmeya.h`**) |
| Compile | Deserialize-only builds can omit **`ZMEYA_ENABLE_SERIALIZE_SUPPORT`** | Same: read-only types and validation headers always available; serialize code is behind the macro |

**Typical code (both):** load bytes, cast to **`const MyRoot*`**, use **`find`**, **`c_str()`**, range **`for`** over arrays where the read API exposes iterators or raw pointers.

**Caveat:** blobs that contain **`HashMap` / `HashSet`** and were written with **`main`** are **not** byte-compatible with blobs from the branch (appendix). Same C++ read *API names*, different on-disk table encoding.

---

## 3. User story: "I build a blob once and ship the bytes"

### Before (`main`): **`BlobBuilder`** session

- You create **`std::shared_ptr<zm::BlobBuilder>`**.
- You **`allocate<Root>()`** into **`zm::BlobPtr<Root>`** (typed view + **`weak_ptr`** back to the builder).
- You fill fields; for **`zm::`** containers you usually call **`blobBuilder->copyTo(...)`** or **`resizeArray`** + **`copyTo`** on slices.
- You **`finalize()`** into **`zm::Span<char>`**, then copy bytes out (tests use a helper like **`utils::copyBytes`**).

**Minimal POD-style root (from `ZmeyaTest01` on `main`):**

```cpp
std::vector<char> bytesCopy;
{
    std::shared_ptr<zm::BlobBuilder> blobBuilder = zm::BlobBuilder::create(1);
    zm::BlobPtr<SimpleTestRoot> root = blobBuilder->allocate<SimpleTestRoot>();

    root->a = 13.0f;
    root->b = 1979;
    // ... plain members ...

    zm::Span<char> bytes = blobBuilder->finalize();
    bytesCopy = utils::copyBytes(bytes);
    std::memset(bytes.data, 0xFF, bytes.size); // builder storage is dead after you copy
}
const SimpleTestRoot* rootCopy = reinterpret_cast<const SimpleTestRoot*>(bytesCopy.data());
```

**What you carry while building:** **`BlobPtr<Root>`** (and other **`BlobPtr<T>`** for allocated children). Assigning into **`zm::Pointer`** often goes through **`blobBuilder->assignTo`** (see list / graph tests on `main`).

### After (`reimplement_builder`): **`write_scope`** session

- One call **`zm::write_scope<Root>(lambda)`** owns the session.
- The lambda gets **`zm::BlobWriter<Root>& w`**.
- **`w.root()`** returns **`zm::ArenaRef<Root>`** (not a raw **`Root*`**). Prefer a **named, explicit** binding, for example **`zm::ArenaRef<SimpleTestRoot> root = w.root();`** so the type is obvious at the call site (avoid **`auto`** here: the RHS alone does not spell **`ArenaRef`**).
- **`w.allocate<T>()`** returns **`zm::ArenaRef<T>`**. Use **`r->field`** / **`n->payload`** for normal access (**`operator->`** re-resolves through the live arena on each use, so it stays valid across **`std::vector<char>`** realloc).
- Call **`transient_ptr()`** when you need a raw **`T*`** for **`zm::Pointer`**, **`std::vector<T*>`**, **`contains_pointer`**, **`validate(const T*)`**, or other APIs that take a plain pointer. **Do not** store that raw pointer across steps that can grow the arena.
- **`zm::`** fields often accept **STL-shaped values via `operator=`** (e.g. **`root->arr1 = std::vector<...>`**) or **`zm::assign`** when you need an explicit builder.
- Return type is **`zm::BlobBuffer`** (aligned **`std::vector<char>`**-like); you **move** it out, no separate **`Span`** + manual copy unless you want one.

**Same logical test (aligned with current `ZmeyaTest01`):**

```cpp
zm::BlobBuffer bytesCopy = zm::write_scope<SimpleTestRoot>(
    [](zm::BlobWriter<SimpleTestRoot>& w)
    {
        zm::ArenaRef<SimpleTestRoot> root = w.root();
        root->a = 13.0f;
        root->b = 1979;
        // ... plain members ...
    });

const SimpleTestRoot* rootCopy = reinterpret_cast<const SimpleTestRoot*>(bytesCopy.data());
```

**Explicit-builder tests** that use **`zm::detail::Builder<Root>::create()`** and **`ScopedBuilder`**: **`builder->getRoot()`** also returns **`zm::ArenaRef<Root>`**; use **`root->...`** or **`root.transient_ptr()`** the same way.

**Implementation note:** **`ArenaRef`** lives in **`Zmeya/Serialize/ZmeyaArenaRef.h`** (serialize builds). It stores the active **`BuilderBase*`** plus a **byte offset** into the arena so the address is re-derived after realloc.

---

## 4. User story: "I add a compound type to my root"

**Compound type** here means a **`struct`** used inside the blob: either a plain POD block or a type that contains **`zm::String`**, **`zm::Array<...>`**, **`zm::HashMap<...>`**, **`zm::Pointer<...>`**, etc.

### Before (`main`)

1. Define the struct with **`zm::`** fields as today.
2. In the build session, you still **`allocate<Root>()`**.
3. For each **`zm::`** member, you **do not** usually write `root->field = std::vector<...>`; you call **`blobBuilder->copyTo(root->field, stlSource)`** (or **`resizeArray`** then **`copyTo`** on nested arrays).
4. For **`zm::Pointer`**, you **`allocate` child objects**, then **`blobBuilder->assignTo(root->ptr, childPtr)`** (or **`operator=`** from **`BlobPtr`** where supported).

**Arrays of aggregates** often use a **temporary STL model** (`std::vector<YourPOD>`), then **`copyTo`**, or use **`resizeArray`** + per-element **`copyTo`** on sub-arrays (see `main` **`ZmeyaTest03`**).

### After (`reimplement_builder`)

1. Define the struct the same way.
2. Inside **`write_scope`**, bind the root with an explicit **`zm::ArenaRef<Root>`** (example: **`zm::ArenaRef<ArrayTestRoot> root = w.root();`**).
3. Prefer **value-like assignment**: **`root->names = std::vector<std::string>{...}`**, **`root->items = std::vector<...>`**, **`root->map = std::unordered_map<...>`** when **`operator=`** is wired for your element type (often via a **template `operator=`** on the element struct that forwards to **`zm::`** members, as in **`ZmeyaTest01`** **`Desc`** + **`root->arr = tempDescs`**).
4. For **`zm::Pointer`**, use **`zm::ArenaRef<T> node = w.allocate<T>();`**, fill **`node->...`**, then **`root->ptr = node.transient_ptr()`** (or push **`node.transient_ptr()`** into **`std::vector<T*>`** for **`Array<Pointer<T>>`** bulk assign).

**Arrays example (current `ZmeyaTest03` style):** bulk assign from **`std::vector`** / nested vectors in one expression where possible:

```cpp
zm::BlobBuffer bytesCopy = zm::write_scope<ArrayTestRoot>([](zm::BlobWriter<ArrayTestRoot>& w) {
    zm::ArenaRef<ArrayTestRoot> root = w.root();
    root->arr1 = std::vector<Payload>{{1.3f, 13}, {2.7f, 27}};
    root->arr2 = std::vector<int32_t>{2, 4, 6, 10, 14, 32};
    root->arr4 = std::vector<std::vector<float>>{
        {1.2f, 2.3f}, {7.1f, 8.8f, 3.2f}, {16.0f, 12.0f, 99.5f, -143.0f}, {-1.0f}};
    std::vector<Payload*> ptrs;
    for (...) {
        zm::ArenaRef<Payload> p = w.allocate<Payload>();
        p->a = ...;
        ptrs.push_back(p.transient_ptr());
    }
    root->arr5 = ptrs;
});
```

Helpers that fill a root can take **`const zm::ArenaRef<TestRoot>&`** so call sites pass **`w.root()`** without converting to raw **`TestRoot*`** first (see **`ZmeyaTestNewAPI.cpp`** **`FillBasicTestRoot`**).

**Authoring ergonomics:** the branch pushes you toward **"build STL-side, assign once"** for complex graphs, matching many game tools. **`main`** pushes you toward **"call `copyTo` / `resizeArray` with the builder in hand"**.

---

## 5. User story: "I load a full hash table from STL"

### Before (`main`)

```cpp
std::shared_ptr<zm::BlobBuilder> bb = zm::BlobBuilder::create(1);
zm::BlobPtr<HashMapTestRoot> root = bb->allocate<HashMapTestRoot>();

std::unordered_map<int, float> testMap = {{3, 7.0f}, {4, 17.0f}, /* ... */};
bb->copyTo(root->hashMap1, testMap);

zm::Span<char> bytes = bb->finalize();
```

### After (`reimplement_builder`)

```cpp
zm::BlobBuffer blob = zm::write_scope<HashMapTestRoot>([](zm::BlobWriter<HashMapTestRoot>& w) {
    zm::ArenaRef<HashMapTestRoot> root = w.root();
    std::unordered_map<int, float> testMap = {{3, 7.0f}, {4, 17.0f}, /* ... */};
    root->hashMap1 = testMap; // or zm::assign(root->hashMap1, testMap);
});
```

**Read-side API** like **`find(key, defaultVal)`** is still the comfort API for maps.

**Performance note (author-facing):** bulk **`operator=`** or **`zm::assign(..., std::unordered_map)`** remains the right tool when you already have the whole map. Per-key **`insert`** during the session is for **incremental** workflows (next section).

---

## 6. User story: "I grow strings, arrays, or maps while building" (mostly **after**)

On **`main`**, the pleasant path is **bulk write**: size arrays via **`resizeArray`**, fill with **`copyTo`**, and treat many **`zm::`** fields as **write-once** for that session.

On **`reimplement_builder`**, the same bulk path exists, **plus** std-like **incremental** APIs during **`write_scope`**:

- **`zm::String`**: **`append`**, **`operator+=`**, **`clear`**
- **`zm::Array<T>`** (when supported for **`T`**): **`push_back`**, **`pop_back`**, **`clear`**, **`erase_at`**, **`resize`**
- **`zm::HashSet` / `zm::HashMap`**: **`insert`**, **`erase`**, **`clear`**, and **`hashmap_reserve_nodes` / `hashset_reserve_nodes`** style hints

You can spell these as **member calls** (TLS-backed) or as explicit **`w.string_append`**, **`w.array_push_back`**, **`w.hashmap_insert`**, etc., on **`BlobWriter`** (**`AGENTS.md`** recommends the **`w.*`** style for clarity).

**Realloc rule (updated):** **`zm::ArenaRef<T>`** through **`w.root()`** / **`w.allocate<T>()`** is safe for repeated **`ref->field`** access across growth because each **`operator->`** re-resolves. A raw **`T*`** from **`transient_ptr()`** is **not** safe to cache across allocator growth; take **`transient_ptr()`** again at the use site, or keep working through **`ArenaRef`**.

---

## 7. User story: "I link objects with `zm::Pointer`"

### Before (`main`)

- Allocate **`Payload`** with **`blobBuilder->allocate<Payload>(...)`** returning **`BlobPtr<Payload>`**.
- Store into **`zm::Array<zm::Pointer<Payload>>`** with **`operator=`** from **`BlobPtr`** or **`assignTo`**.

### After (`reimplement_builder`)

- **`zm::ArenaRef<Payload> p = w.allocate<Payload>();`** then assign fields on **`p`**.
- Fill **`zm::Array<zm::Pointer<Payload>>`** from **`std::vector<Payload*>`** using **`p.transient_ptr()`** in the vector, or assign **`elem = other.transient_ptr()`** for single **`Pointer`** slots.

**Safety:** the branch adds **bounded** **`try_get_in_blob`** on **`Pointer`** for untrusted buffers; hot **`get()`** still asserts on bad integration data unless you validated first.

---

## 8. User story: "I validate untrusted input"

### Before (`main`)

Most teams used **trust-but-verify** manually (size checks, casting only after length checks).

### After (`reimplement_builder`)

First-class **blob validators**, strict **`as_root_blob_strict`**, optional **deep** validation hooks via **`blob_root_deep_validate<Root>`** specialization (**`ZmeyaBlobValidate.h`**). This is primarily an **integration / tools** story, not a rename of the read API.

---

## 9. Side-by-side cheat sheet (author actions)

| Task | `main` | `reimplement_builder` |
|------|--------|------------------------|
| Start a build | **`BlobBuilder::create(n)`** | **`zm::write_scope<Root>(...)`** |
| Hold root while building | **`zm::BlobPtr<Root>`** | **`zm::ArenaRef<Root>`** from **`w.root()`** (use explicit type; **`ref->field`** re-resolves) |
| Allocate extra **`T`** in blob | **`allocate<T>(...)`** returns **`BlobPtr<T>`** | **`w.allocate<T>()`** returns **`zm::ArenaRef<T>`**; use **`transient_ptr()`** for **`T*`** sinks |
| Fill **`zm::Array` from STL** | **`copyTo(field, vec)`**, **`resizeArray`**, **`getArrayElement` + `copyTo`** | **`field = vec`**, nested **`std::vector<std::vector<...>>`**, or **`zm::assign`** |
| Fill **`zm::HashMap` from STL** | **`copyTo(field, unordered_map)`** | **`field = unordered_map`** (or **`assign`**) |
| Append to string / push row | Mostly "assign whole value again" patterns | **`append` / `push_back` / `insert`** (incremental) |
| Finish | **`finalize()`** then copy **`Span<char>`** | **`return` move of `BlobBuffer`** from **`write_scope`** |
| Tests that force realloc stress | ad hoc tiny initial size | **`zmeya_test::write_scope_stressed`** (**`TestHelper.h`**) |

---

## 10. Appendix A: build system (short)

- **`main`**: drop **`Zmeya.h`** into your tree; list it in CMake if you want parse headers behavior.
- **`reimplement_builder`**: depend on **`Zmeya` INTERFACE** target (**`add_subdirectory` + `target_link_libraries(... Zmeya)`**).

---

## 11. Appendix B: binary compatibility

- **`HashSet` / `HashMap`** on-disk representation **changed** (range-based buckets on **`main`** vs **separate chaining** on the branch). Treat cross-version blobs as **different file formats** unless you run a dedicated migration tool.
- **Golden tests:** comparing full **`memcmp`** of blobs across versions is usually **not** meaningful for maps/sets; compare **logical** contents after load.

---

## 12. Appendix C: where to read more in-repo

- **`AGENTS.md`**: TLS, **`write_scope`**, **`ArenaRef`**, **`transient_ptr()`**, realloc rules, incremental APIs, **`BlobWriter`** vs **`assign(builder, ...)`**.
- **`README.md`**: high-level mental model and minimal **`write_scope`** example (**`zm::ArenaRef<MyRoot> r = w.root()`**).
- **`docs/ZmeyaWritePathDesign.md`**: normative design for the write path.

---

*Document focus: end-user / integrator perspective. For exhaustive internal diffs, use `git diff main...reimplement_builder`.*

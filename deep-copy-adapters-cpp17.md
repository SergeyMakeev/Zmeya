# Deep Copy Adapters for Converting `std::*` to `my::*` (C++17)

**Last updated:** 2025-08-09 20:42:16

This document explains a C++17 technique to **deep‑copy** arbitrary `std` containers into custom `my` containers (and vice versa if you add the reverse adapters) **without writing nesting‑aware converters**. The pattern relies on a small set of **shape adapters** for container types and **leaf converters** for element types. Arbitrary nesting then falls out naturally via **recursion**.

---

## 1) The original problem

You have custom containers and types:

```cpp
namespace my {
    template<class T> struct vector { /* reserve, clear, push_back... */ };
    struct string { /* ... */ };
    template<class K, class V> struct unordered_map { /* reserve, clear, emplace... */ };
}
```

You want to convert things like:

```cpp
std::vector<std::string>         in1;
my::vector<my::string>           out1;

std::unordered_map<std::string, std::vector<int>>          in2;
my::unordered_map<my::string, my::vector<long long>>       out2;
```

The naïve solution is to write a custom converter for **every** nesting you care about: vector of strings, vector of vector of strings, map from string to vector of strings, etc. That approach scales **quadratically** with combinations and is unmaintainable.

**Goal:** Write only a few generic pieces so that **any nesting** (vector of vector of map of pair...) is supported automatically.

---

## 2) The idea

Split conversion into two layers:

- **Leaf converters** (type adapters): convert *individual* values (e.g., `std::string → my::string`). There are relatively few of those.
- **Shape converters** (container adapters): convert container *shape* (e.g., `std::vector<F> → my::vector<T>`). These do **not** know anything about `F`/`T`—they simply **recurse** using the same function for elements (and keys/values for maps).

Because shape converters call `deep_copy` recursively on elements, **arbitrary nesting** works without any container knowing the full depth of the structure.

---

## 3) C++17 reference implementation

The following snippet is self‑contained and C++17‑compatible (no concepts). It uses overloads + SFINAE and shows the pattern for `std::vector → my::vector` and `std::unordered_map → my::unordered_map`. Add similar shapes as needed.

```cpp
#include <vector>
#include <unordered_map>
#include <string>
#include <type_traits>
#include <utility>

namespace my {
    template<class T>
    struct vector {
        void reserve(size_t) {}
        void clear() {}
        void push_back(const T&) {}
        void push_back(T&&) {}
        // ...
    };

    struct string {
        void assign(const char* s, size_t n) { (void)s; (void)n; }
        // ...
    };

    template<class K, class V>
    struct unordered_map {
        void reserve(size_t) {}
        void clear() {}
        template<class KK, class VV>
        void emplace(KK&&, VV&&) {}
        // ...
    };
}

// ---------------- base cases ----------------

// Identity copy when assignable (SFINAE on the assignment expression)
template<class T>
inline auto deep_copy(const T& from, T& to)
    -> decltype(to = from, void())
{
    to = from;
}

// F -> T via constructing T(from), when constructible and not the same type
template<class F, class T,
         class = typename std::enable_if<
             !std::is_same<F, T>::value &&
             std::is_constructible<T, const F&>::value
         >::type>
inline void deep_copy(const F& from, T& to) {
    to = T(from);
}

// Leaf specialization: std::string -> my::string
inline void deep_copy(const std::string& from, my::string& to) {
    to.assign(from.data(), from.size());
}

// ---------------- container shapes ----------------

// std::vector<F> -> my::vector<T>
template<class F, class T>
inline void deep_copy(const std::vector<F>& from, my::vector<T>& to) {
    to.clear();
    to.reserve(from.size());
    for (const auto& x : from) {
        T y{};
        deep_copy(x, y);              // recurse on element
        to.push_back(std::move(y));
    }
}

// std::unordered_map<KF,VF> -> my::unordered_map<KT,VT>
template<class KF, class VF, class KT, class VT>
inline void deep_copy(const std::unordered_map<KF, VF>& from,
                      my::unordered_map<KT, VT>& to) {
    to.clear();
    to.reserve(from.size());
    for (const auto& kv : from) {
        KT k{}; VT v{};
        deep_copy(kv.first,  k);      // recurse on key
        deep_copy(kv.second, v);      // recurse on value
        to.emplace(std::move(k), std::move(v));
    }
}

// ------------- optional helper (return-by-value) -------------
template<class To, class From>
inline To to_my(const From& from) {
    To out{};
    deep_copy(from, out);
    return out;
}
```

**Usage:**

```cpp
std::vector<std::string> from = {"a", "bb", "ccc"};
my::vector<my::string> to;
deep_copy(from, to);  // deep-copies strings and vector shape recursively
```

---

## 4) Why this works

1. **Overload selection** picks the most specific viable `deep_copy` for the source/target pair.
2. **Leaf vs shape separation** means only a handful of shape adapters are needed (`vector`, `map`, `pair`, `optional`, `tuple`, etc.).
3. **Recursion on elements** handles arbitrary depth: the vector converter calls `deep_copy` on each element; if an element is itself a container, the corresponding shape adapter triggers again; if it’s a leaf, the leaf converter runs.
4. **SFINAE guards** avoid ambiguity between identity, constructible, and specialized overloads. The “constructible” fallback only participates if the types differ and the target is constructible from the source.
5. **No nesting awareness**: a vector of maps of vectors “just works” because each layer delegates to the same mechanism.

---

## 5) Extending to support new types

### 5.1 New leaf type conversions
Add a **non‑template** overload (or a constrained template with SFINAE) for a leaf pair:

```cpp
// std::u16string -> my::string
inline void deep_copy(const std::u16string& from, my::string& to) {
    // convert UTF‑16 to UTF‑8 or your internal encoding
    // to.assign(...);
}
```

Rule of thumb: leaf converters should **not** call `deep_copy` recursively.

### 5.2 New container shapes
For each new container family, write a **shape adapter** that:
- clears the target
- reserves capacity if available
- iterates source elements
- creates target elements (default‑construct), then calls `deep_copy` recursively
- inserts/emplaces the converted element into the target

Examples to add:

```cpp
// std::array<F, N> -> my::vector<T>
template<class F, class T, std::size_t N>
void deep_copy(const std::array<F, N>& from, my::vector<T>& to);

// std::map<KF, VF> -> my::unordered_map<KT, VT>
template<class KF, class VF, class KT, class VT>
void deep_copy(const std::map<KF, VF>& from, my::unordered_map<KT, VT>& to);

// std::pair<AF, BF> -> my::pair<AT, BT>
template<class AF, class BF, class AT, class BT>
void deep_copy(const std::pair<AF, BF>& from, my::pair<AT, BT>& to);

// std::optional<F> -> my::optional<T>
template<class F, class T>
void deep_copy(const std::optional<F>& from, my::optional<T>& to);
```

Each shape adapter is small; the recursion does the heavy lifting.

### 5.3 Reverse direction
Add the symmetric overloads (`my::*` → `std::*`) if you need bi‑directional conversion. The pattern is identical.

---

## 6) Design options and trade‑offs

- **Fallbacks:** The constructible fallback `to = T(from)` is convenient but can mask mistakes. If you prefer stricter control, remove the fallback and require explicit leaf adapters.
- **Identity assignment:** The `decltype(to = from, void())` trick only participates if `T` is assignable from `T`. This avoids incorrect matches for types without assignment.
- **Move‑aware deep copy:** You can add `deep_copy(F&&, T&)` overloads to enable moving where source is an rvalue, but be careful to not accidentally move from elements you still need.
- **Exception safety:** Reserve before inserting; clear first for strong safety. If target supports `assign`/`insert_range`, prefer those.
- **ADL:** Keep `deep_copy` at namespace scope (not inside `my`) so both `std` and `my` arguments can find it via normal lookup and your TU can add overloads for third‑party types.
- **Headers/ODR:** Put declarations in a header. If you add function‑local statics or any state, ensure ODR safety across TUs.
- **Non‑copyable elements:** If elements are non‑copyable but convertible, rely on the leaf converter to construct the `T` in place and then `emplace` it.

---

## 7) Testing strategy

- **Leaf tests:** Verify each specific leaf conversion (`std::string → my::string`), including empty and large inputs.
- **Shape tests:** Small, medium, and large sizes; validate `reserve` is called (if observable) and result size matches.
- **Nested tests:** Two and three layers deep (e.g., `vector<map<string, vector<int>>>`). Ensure values are preserved.
- **Negative tests:** Types that should **not** convert—confirm they fail to compile (SFINAE blocks) or static_assert a helpful message.

A useful pattern is a helper that returns by value, which makes tests concise:

```cpp
auto out = to_my<my::vector<my::string>>(std::vector<std::string>{"x","yy"});
```

---

## 8) Common pitfalls (and fixes)

1. **Ambiguous overloads** between identity and constructible fallback.  
   *Fix:* add the `!std::is_same<F,T>::value` guard on the fallback.
2. **Catch‑all templates stealing matches** (too generic).  
   *Fix:* prefer **non‑template** leaf overloads for specific type pairs; keep templates narrowly constrained.
3. **Containers without `reserve`/`emplace`**.  
   *Fix:* use what’s available; wrap calls in small traits if you need a generic reserve/emplace protocol.
4. **Hidden conversions** doing the wrong thing.  
   *Fix:* remove the constructible fallback, or restrict it to explicit `std::is_convertible`/`std::is_constructible` combinations you trust.

---

## 9) Minimal checklist for adding a new container shape

- [ ] Clear the target (`to.clear()`)
- [ ] Pre‑allocate if possible (`to.reserve(from.size())`)
- [ ] Loop over elements/entries
- [ ] Default‑construct target element(s)
- [ ] `deep_copy` recursively into them
- [ ] Insert/emplace them into the target
- [ ] Write a small test (also a nested test)

---

## 10) Appendix: fully inlined example

```cpp
std::vector<std::string> from = {"a", "bb", "ccc"};
my::vector<my::string> to;
deep_copy(from, to);
```

No container converter here knows about strings or depth; the recursion and leaf adapters do all the work.

---

**License:** Use, copy, and modify freely within your project.

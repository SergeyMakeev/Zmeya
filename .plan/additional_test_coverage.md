# Additional Zmeya test coverage backlog

This document lists **planned** tests (not implemented here). Each item is meant to catch regressions, edge cases, or undefined behavior around **blob construction**, **incremental mutation**, **compaction/finalize**, and **read-side layout**.

**How to use:** pick items by priority (P0 first), implement in `ZmeyaTestIncremental.cpp` / `ZmeyaTestNewAPI.cpp` or a new focused file, keep **golden vs incremental** or **byte-for-byte** comparisons where cheap.

**Conventions in this file:** *golden* means bulk `operator=` / single assign from STL model; *incremental* means `BlobWriter` APIs or repeated small mutations.

---

## P0: Correctness hazards (memory, reallocation, compaction)

| ID | Name / idea | Description | Implementation plan |
|----|----------------|-------------|----------------------|
| P0-01 | String append after many reallocs | `string_append_cstr` must not use raw `char*` into the vector across `alloc_aligned` growth; stress with tiny initial reserve and long chain of appends. | `write_blob` with initial size 16..64; loop append 500 times single-char or random-length chunks; compare to one `std::string` built the same way then assigned once. |
| P0-02 | String clear then append | Cleared string has null `Pointer`; re-assign and append must match fresh string. | `string_clear`; then `string_append` and `operator=`; compare golden. |
| P0-03 | Empty string assign and append | `text = ""` then append non-empty; avoid strlen/UB on empty. | Golden `std::string` empty then +=; incremental clear + append. |
| P0-04 | Array push_back across realloc boundary | Same class of bug as strings: capture old slab only by **offset** after growth if any code path still uses stale pointer (regression guard). | Tiny reserve; push 200+ ints; golden `vector`; memcmp or element-wise. |
| P0-05 | Array push_back first element | First push from empty allocates default capacity (8); verify size and value. | Single `array_push_back`; expect one element; golden match. |
| P0-06 | Array erase_at boundaries | Erase index 0, last, middle; verify size and order. | Deterministic sequence vs golden vector built with same ops in STL. |
| P0-07 | Array pop_back until empty | Repeated pop; no-op when empty. | Loop pop 11 times after 10 pushes; expect empty; no crash. |
| P0-08 | Array resize grow shrink | `array_resize` up then down; verify tail truncation semantics match `array_resize_fill` contract. | Document expected behavior from code; assert sizes and fill value in grown tail. |
| P0-09 | Compaction with HashMap String keys | Freed key blobs must not leave stale `roffset` registry entries (historical bug). | Many `hashmap_insert` / `hashmap_erase` with `std::string` keys; small reserve; optional `peak_size` vs `finalized_size`; logical map equality to golden. |
| P0-10 | Compaction many disjoint dead ranges | Interleave string replaces and array slab replaces; finalize must remap all slots. | Mixed root: several strings + `Array<int>` push cycles; compare digest of final blob or logical values. |
| P0-11 | Finalize alignment matrix | Wrong alignment can leave inconsistent padding or violate consumer assumptions. | Same minimal blob written with `finalizeAlignment` in `{1,2,4,8,16,32}`; assert `blob.size() % align == 0` and payload still parses. |
| P0-12 | Explicit `assign(BuilderBase&, ...)` under realloc | Nested string assign during hash rebuild must see valid TLS (`ScopedBuilder` paths). | `detail::Builder` + `ScopedBuilder`; `assign(builder, map, ...)` with values triggering inner `String` assign; match `write_blob` golden. |
| P0-13 | `deep_copy(detail::BuilderBase&, ...)` nesting | Second overload used when TLS must be scoped per builder. | Allocate side object with `deep_copy(builder, from, offset)` where `from` contains nested strings; read back. |
| P0-14 | `allocate()` pointer graph under stress | `w.allocate` nodes then wire `Pointer`s; validate after finalize. | Similar to `PointerTest` but with 32-byte reserve and more nodes to force realloc. |
| P0-15 | `contains_pointer` API | BlobWriter exposes containment; misuse should be detectable in tests. | Allocate object; `EXPECT_TRUE(writer.contains_pointer(p))`; stack variable `EXPECT_FALSE`. |

---

## P1: Hash containers (logic, collisions, types)

| ID | Name / idea | Description | Implementation plan |
|----|----------------|-------------|----------------------|
| P1-01 | HashSet String incremental vs bulk | Mirror `IncrementalHashMap` for `HashSet<zm::String>`. | Golden `unordered_set<string>` assign; incremental `hashset_insert` in random order; sort strings compare. |
| P1-02 | HashMap non-String key incremental | Integer keys use different hash path than `hashString`. | `HashMap<int32_t, int32_t>` insert/erase/overwrite; compare to `unordered_map`. |
| P1-03 | HashMap String value with incremental | Values are `zm::String`; nested assign stress. | `HashMap<int, zm::String>` or `HashMap<zm::String, zm::String>`; golden vs incremental. |
| P1-04 | Duplicate key insert | Second insert with same key must replace value (STL map behavior). | Insert key K twice with different ints; read final value. |
| P1-05 | Erase missing key | Must be no-op / not corrupt. | Erase nonexistent; size unchanged. |
| P1-06 | Clear then refill | `hashmap_clear` / `hashset_clear` then inserts. | Two waves of data; verify only second wave present. |
| P1-07 | Collision-heavy keys | Many distinct strings with same hash bucket (if test hash hook exists) OR many keys modulo small bucket count. | If `ZMEYA_EXTERNAL_HASH` testable, inject; else insert N=1000 sequential strings; still equals golden map. |
| P1-08 | Single-element maps and sets | Minimum bucket layouts. | One insert; iterate; find. |
| P1-09 | Large N incremental hash | Performance + correctness; catches O(N^2) rebuild bugs. | N=5000 inserts incremental vs golden (may need larger timeout only in Release). |

---

## P2: Strings (encoding, size, operators)

| ID | Name / idea | Description | Implementation plan |
|----|----------------|-------------|----------------------|
| P2-01 | Long string near builder stress | Long but below practical `roffset` overflow; ensures growth path stable. | 100k or 1M char string assign (Release-only if slow); golden compare. |
| P2-02 | String with embedded NUL | `std::string` with `'\0'` middle; Zmeya stores C-style; document expected behavior. | If library uses strlen end-to-end, document truncation risk; test actual behavior vs `size()`-aware API if added later. |
| P2-03 | Assign from `const char*` vs `std::string` | Both entry points for same bytes. | Two blobs; `strcmp` equal. |
| P2-04 | Chained `operator+=` only | No bulk assign until end. | Only `+=` in loop; tiny reserve. |
| P2-05 | `String::clear()` from member API | Uses TLS builder. | After assign, `clear()`; empty `c_str()` is `""`. |
| P2-06 | Comparison operators | `==`, `!=` with `const char*`, `std::string`, reflexive cases. | Small table-driven EXPECT. |
| P2-07 | Self-alias assign | Assign string from `c_str()` of itself if API allows; or two zm strings same payload. | Define whether supported; if not, assert or skip with comment. |

---

## P3: Arrays and nested containers

| ID | Name / idea | Description | Implementation plan |
|----|----------------|-------------|----------------------|
| P3-01 | `Array<zm::String>` incremental | Not `zm_array_push_back_ok` for String; must use assign path not raw push. | Document: bulk assign only; negative test that push_back fails compile OR runtime assert if attempted. |
| P3-02 | `Array<zm::Pointer<T>>` assign | Graph of pointers in array. | `vector` of indices into allocated nodes; match PointerTest style. |
| P3-03 | Nested `Array<Array<int>>` | 2D grid assign vs double loop push if API exists. | Bulk assign from `vector<vector<int>>`; incremental only if push is supported for inner arrays (likely assign-only). |
| P3-04 | Maximum `uint32_t` elements guard | `numElements` is uint32; test near UINT32_MAX only if safe in test harness (probably skip or use smaller boundary 4e9 not realistic). | Prefer boundary: resize to `1u<<20` with fill -1 in Release only. |
| P3-05 | `array_erase_at` on empty | Out-of-range index; must no-op. | `erase_at(arr, 0)` on empty; no crash. |

---

## P4: Pointers, graphs, allocate

| ID | Name / idea | Description | Implementation plan |
|----|----------------|-------------|----------------------|
| P4-01 | Null `Pointer` assign | `root->p = nullptr`; registered slot target 0. | Read relative offset is zero; no crash on deserialize read. |
| P4-02 | Long chain list via Pointer | Linked list 1000 nodes; validates roffset chains. | Allocate in order; walk `get()` chain. |
| P4-03 | DAG with multiple parents | Not only cycle like PointerTest; tree with back-pointer. | Binary tree parent pointer + children array. |
| P4-04 | `allocate` alignment | Verify returned pointers aligned to `alignof(T)` for several T. | `uintptr_t(p) % alignof(T) == 0` for `double`, struct with over-alignment if any. |

---

## P5: Finalize, padding, read-only blob

| ID | Name / idea | Description | Implementation plan |
|----|----------------|-------------|----------------------|
| P5-01 | Blob size with alignment 1 | No padding added when already aligned. | Root size multiple of 1 trivial; assert sizes. |
| P5-02 | Odd-size root with align 8 | Forces padding bytes non-zero init (zeros). | Write blob; last padding bytes should be zero (memcmp tail). |
| P5-03 | Deserialize-only build | If CI can build without `ZMEYA_ENABLE_SERIALIZE_SUPPORT`, ensure read tests still compile. | Optional second target in CMake (future); document. |
| P5-04 | Span returned from finalize | Internal `Span<char>` length matches vector after finalize (unit-test via duplicate write_blob). | Compare returned size to `vector::size` indirectly by file round-trip. |

---

## P6: I/O, mmap, files

| ID | Name / idea | Description | Implementation plan |
|----|----------------|-------------|----------------------|
| P6-01 | Mmap with non-default finalize alignment | Current test may use one alignment; vary alignment vs mmap read path. | Write with align 16; mmap; validate magic. |
| P6-02 | Empty file / truncated file | Reader should fail safe (if APIs exist). | Document behavior; add test if error path exists. |
| P6-03 | SimpleFileTest cross-platform | Today may be Windows-centric; ensure Linux path in CI. | Guard with `#ifdef` or portable temp file API. |
| P6-04 | Write blob to disk and read back | Full round-trip byte identity. | `write_blob` to `vector`, fwrite, fread, memcmp. |

---

## P7: ReferTo-style and large blobs

| ID | Name / idea | Description | Implementation plan |
|----|----------------|-------------|----------------------|
| P7-01 | Reduce node count variant of ReferToTest | Faster Debug CI with 100 nodes same validation pattern. | Parameterized gtest `INSTANTIATE_TEST_SUITE_P` or constant kNodes. |
| P7-02 | Single-node ReferTo | Minimal deep copy of all field kinds. | One node with string, array, hashset, hashmap. |
| P7-03 | ListTest smaller Debug variant | `ZmeyaTest04` is heavy; optional quick smoke list size 10 in Debug. | `ifdef _DEBUG` smaller N. |

---

## P8: Iterator and read-side API

| ID | Name / idea | Description | Implementation plan |
|----|----------------|-------------|----------------------|
| P8-01 | Empty container iteration | `HashSet` / `HashMap` / `Array` begin/end empty no crash. | Range-for over empty; count 0. |
| P8-02 | Iterator equality end | Two end iterators compare equal. | Standard iterator loop patterns from IteratorsTest expanded. |
| P8-03 | `find` default value on missing | HashMap `find(key, default)` path. | Already in ReferTo; add dedicated tiny test. |

---

## P9: BuilderBase / detail API (advanced)

| ID | Name / idea | Description | Implementation plan |
|----|----------------|-------------|----------------------|
| P9-01 | `note_dead_range` idempotency | Multiple notes same range; compact once. | Manual dead notes (if exposed) OR string churn; assert no double-free / assert in compact. |
| P9-02 | `merge_intervals` ordering | Overlapping and adjacent dead intervals. | Indirect via many overlapping string replacements; final size monotonic. |
| P9-03 | `register_roffset_slot` overwrite | Same slot registered twice with new target. | String assign twice; patch phase still valid. |
| P9-04 | `builder_base()` from BlobWriter | Non-null during callback. | `EXPECT_NE(w.builder_base(), nullptr)`. |

---

## P10: Negative / assertion tests (death tests or ifdef)

| ID | Name / idea | Description | Implementation plan |
|----|----------------|-------------|----------------------|
| P10-01 | `assign` without TLS | Calling `assign` outside `write_blob` should assert (current design). | `EXPECT_DEATH` or platform-specific; skip if `ZMEYA_ASSERT` does not abort in test build. |
| P10-02 | `string_append` nullptr | Must assert before strlen. | Death test with `ASSERT_DEATH` for `writer.string_append(s, nullptr)` if supported on MSVC. |
| P10-03 | `finalize` alignment 0 | Invalid; assert. | Death test `alignment=0` via friend/internal test hook or document-only if not exposable. |
| P10-04 | `get_relative_offset` stack pointer | If test can construct call with stack base (hard), expect assert. | Low value; document UB instead. |

---

## P11: Cross-feature integration roots

| ID | Name / idea | Description | Implementation plan |
|----|----------------|-------------|----------------------|
| P11-01 | One root with all container kinds | Single blob touches string, array, hashmap, hashset, pointer, nested array. | One `write_blob` lambda filling every field; read everything. |
| P11-02 | MMapTest-shaped tree built incrementally | Mix incremental and bulk in same blob. | Half nodes `allocate` + manual fields, half assign from init structs. |
| P11-03 | Version field + forward compatibility | Root starts with magic + version uint32; readers ignore unknown tail (future). | Placeholder test documenting pattern. |

---

## P12: Determinism and golden helpers

| ID | Name / idea | Description | Implementation plan |
|----|----------------|-------------|----------------------|
| P12-01 | Same seed golden twice | Two `write_blob` same lambda; optional byte-identical if allocator deterministic. | `memcmp` two vectors; if non-deterministic bucket order, compare canonicalized logical model instead. |
| P12-02 | Canonicalize HashMap for compare | Sort key-value pairs by key string before EXPECT. | Shared helper in TestHelper for all map tests. |
| P12-03 | Canonicalize HashSet | Sort elements to `vector` then compare. | Already used in incremental hashset test; extract helper. |

---

## P13: Performance smoke (not benchmarks)

| ID | Name / idea | Description | Implementation plan |
|----|----------------|-------------|----------------------|
| P13-01 | Incremental build wall time bound | Catch catastrophic regression (e.g. O(N^3) rebuild). | Release-only `EXPECT_LT(elapsed_ms, threshold)` for fixed N. |
| P13-02 | ListTest size scaling | Document N vs time; optional CI split fast/slow job. | Comment in CMake or AGENTS.md pointer. |

---

## P14: `Pair` and miscellaneous types

| ID | Name / idea | Description | Implementation plan |
|----|----------------|-------------|----------------------|
| P14-01 | `Pair` in blob | If `Pair` used in public roots, assign and read. | Root with `zm::Pair<int, float>`; assign; read fields. |
| P14-02 | Enum class fields | `NodeType` style enums in mmap test; add serialize-side enum round-trip. | Write enum via root; read numeric value. |

---

## P15: Paranoid / compile-flag matrix

| ID | Name / idea | Description | Implementation plan |
|----|----------------|-------------|----------------------|
| P15-01 | Build tests with `ZMEYA_BUILDER_PARANOID` off | Ensure non-paranoid still passes all tests. | Optional CI matrix job toggling define in CMake. |
| P15-02 | `ZMEYA_EXTERNAL_HASH` path | If used in any configuration, add hash injection test. | Conditional compile test file. |

---

## P16: Concurrency (documentation-level)

| ID | Name / idea | Description | Implementation plan |
|----|----------------|-------------|----------------------|
| P16-01 | TLS builder single-threaded contract | Two threads two builders undefined; document no test unless library gains thread support. | Doc-only item in README / this plan. |

---

## Summary counts

Rough backlog size by section: P0 **15**, P1 **9**, P2 **7**, P3 **5**, P4 **4**, P5 **4**, P6 **4**, P7 **3**, P8 **3**, P9 **4**, P10 **4**, P11 **3**, P12 **3**, P13 **2**, P14 **2**, P15 **2**, P16 **1** -> **about 75** distinct test ideas.

**Suggested implementation order:** all **P0**, then **P1** + **P2**, then pick from **P3-P6** based on recent code churn (touch compaction -> P0-09/10 first; touch strings -> P0-01/02 first).

---

## Maintenance

When implementing a row, replace or annotate it in this file with **DONE (test name, file)** or **WONT (reason)** to avoid duplicate work.

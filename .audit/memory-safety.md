# Memory Safety Audit

Audit scope: Zmeya header-only library (`Zmeya/`): self-relative blobs, arena serialization (`ZMEYA_ENABLE_SERIALIZE_SUPPORT`), mmap-oriented read views. Assumes skeptical review of untrusted binary input.

## Audit revision (latest code)

This document was refreshed against **commit `0d7cb68c164d991d301a0dec1a23065c47f0c943`** (*Option C chained hash tables and builder realloc safety*, 2026-05-11). Highlights versus the prior audit baseline:

- **`HashMap` / `HashSet` on-disk layout** switched from **open-addressed bucket slices** (`beginIndex` / `endIndex` into a flat `items` array) to **index chaining** (`buckets[].head`, dense `nodes[]` with `next`, `ZMEYA_HASH_CHAIN_NIL`, `live_count_`, free list for incremental erase). Implementation lives in `ZmeyaHashSet.h`, `ZmeyaHashMap.h`, and `ZmeyaBuilderHashChain.inc` (included from `ZmeyaBuilderBase.h`).
- **Builder-side mitigations:** chain/rehash and assign paths **re-bind** container pointers from stored **`goffset_t`** (`hm_g` / `hs_g`) after **`alloc_aligned`** / growth, with explicit in-file warnings that **`HashMap&` / `HashSet&` into the arena can dangle** for the rest of a call after reallocation. This reduces **use-after-realloc** bugs inside the library but does not change the **untrusted read** story unless metadata is validated.
- **API / lifetime docs:** `ZmeyaArray.h` and `ZmeyaString.h` now document **std-like invalidation** during `write_blob` / `BlobWriter`. `HashSet` / `HashMap` headers document invalidation across **`hashset_*` / `hashmap_*`** mutations. **`HashMap::const_iterator`** now exposes **`std::pair<const Key&, const Value&>`** so **`zm::String` keys are not shallow-copied** during iteration (offsets stay relative to blob slots).
- **Incremental policy:** `zm_hashset_chain_incremental_ok<Key>` and `zm_hashmap_chain_incremental_ok<Key, Value>` gate **O(1) chain** incremental APIs; **`String`** (and other non-trivial keys) still go through **STL snapshot + bulk `impl_assign`** in the serialize API (`ZmeyaSerializeApi.inc`).

The executive summary and several findings below were updated for the chained representation. Items that still apply unchanged (Murmur alignment, `strlen`, TLS, asserts, etc.) are retained.

## Executive Summary

**Overall risk level: High** for any path that treats a Zmeya blob as possibly hostile or externally supplied without a separate validation layer. **Medium** if blobs are strictly self-produced (same version, same toolchain) and never shared across trust boundaries.

Zmeya is a header-only, self-relative, mmap-friendly **view over bytes**. The read-side API (`zm::Array`, `zm::String`, `zm::Pointer`, `zm::HashSet`, `zm::HashMap`) largely **assumes layout integrity**. There is **no centralized parser** that proves offsets, counts, **hash chain indices**, and link structure stay inside the buffer (and acyclic) before you dereference. Dominant failure classes remain **out-of-bounds reads**, **non-terminating walks on corrupt chains**, and **unbounded `strlen`** on malformed data, rather than classic heap double-free in the library itself (the serialize path uses `std::vector` and explicit bookkeeping for dead ranges).

## Highest-Risk Findings

### Finding: Read-side chained hash tables trust link indices (replaces bucket-slice model)

**Severity:** High  
**Category:** buffer overrun / denial of service (untrusted blob) / asset lifetime  
**Location:** `ZmeyaHashSet.h` (`containsImpl`, iterators), `ZmeyaHashMap.h` (`findImpl`, iterators)

**Problem (current code):** Traversal follows `buckets[bucketIndex].head` and `nodes[i].next` until `ZMEYA_HASH_CHAIN_NIL`. There is **no validation** that each index is **`< nodes.size()`**, that `live_count_` matches the number of reachable live nodes, that links are **acyclic**, or that free-list links stay disjoint from bucket chains. Corrupt data can cause **OOB access** on `nodes[i]` or **infinite loops** (CPU hang) on `contains` / `find` / iteration.

**Supersedes (resolved as a distinct bug class):** The previous open-addressed layout exposed **unchecked `beginIndex` / `endIndex` slices** into `items`. That specific slice arithmetic is **gone**; risk moved to **chain integrity**.

**Why it is dangerous:** Same trust issues as before for hostile blobs, plus **algorithmic non-termination** if `next` forms a cycle.

**How to catch earlier:** Validated view types built only after a pass that checks all indices, max chain length bounded by `nodes.size()`, optional **Floyd cycle detection** in debug, and consistency of `live_count_` vs walked nodes.

**Runtime defense:** Debug-only step counters on traversals; release **bounded-step** failure for hardened loaders.

**Recommended fix:** On load, validate: every `head` and every `next` is either `NIL` or `< nodes.size()`; walk each bucket chain with a step cap; verify total visited live nodes equals `live_count_` (and matches policy for free list).

**Test coverage needed:** Mutated blobs: `next` cycle, `head == 0` with spoofed layout, index `>= nodes.size()`, inconsistent `live_count_`.

---

### Finding: `Array::operator[]` and `Pointer::get` are unchecked on the read path

**Severity:** High (untrusted input) / Medium (trusted)  
**Category:** buffer overrun / ownership ambiguity  
**Location:** `ZmeyaArray.h`, `ZmeyaPointer.h`

**Problem:** `operator[]` indexes with **no bounds check** (only `at()` asserts). `Pointer::get()` applies a self-relative offset with **no proof** the target lies inside the blob.

**Why it is dangerous:** Corrupt `numElements` or `relativeOffset` causes **OOB access** or **wild pointers**.

**How to catch earlier:** Split API: `UncheckedArrayView` vs `BoundedArrayView` carrying `std::span<const std::byte> parent` and validating on each access in debug, or accessors returning `std::optional<std::reference_wrapper<const T>>` in hardened builds.

**Runtime defense:** Saturating checks + early return / error type in hardened mode; debug-only poison values after failed validation.

**Recommended fix:** Document explicitly: **trusted blob only** unless wrapped. Provide `bool try_get(span, out)` accessors.

**Test coverage needed:** Negative tests with deliberately broken offsets and counts.

---

### Finding: `strlen` / `strcmp` on blob-backed C strings

**Severity:** High (untrusted)  
**Category:** buffer overrun  
**Location:** `ZmeyaString.h` (`c_str`, comparisons), `ZmeyaHash.h` (`hashString` via `strlen`), incremental paths using `strlen` on arena strings (`ZmeyaBuilderBase.inc` `string_append_cstr`, `string_clear`), **`ZmeyaBuilderHashChain.inc`** (`zm_hash_bucket_index_stored` for `zm::String` uses `k.c_str()` / `HashUtils::hashString`)

**Problem:** String helpers assume a **null-terminated** payload. A truncated blob or missing `'\0'` makes `strlen` read until it walks out of the mapping.

**Why it is dangerous:** Classic **read off the end of the mmapped region**; can fault immediately or read adjacent mappings.

**How to catch earlier:** Store **length-prefixed** strings in the format, or require `StringView{ptr, len}` internal representation for blob strings.

**Runtime defense:** Every string field validated against parent span max length; bounded scan for terminator within `(blob_end - ptr)`.

**Recommended fix:** At serialization time always write `len+1` and validate on load before exposing `c_str()`.

**Test coverage needed:** Truncated string blob (no NUL), NUL at last byte of mapping.

---

### Finding: Murmur hash reads `uint64_t` from arbitrary `const char*`

**Severity:** Medium  
**Category:** misaligned access / strict aliasing  
**Location:** `ZmeyaHash.h` (`murmur_hash_process64a`)

**Problem:** Casts string bytes to `const uint64_t*` and reads word-wise.

**Why it is dangerous:** On platforms that fault on misaligned loads, **unaligned** string starts crash. Strict-aliasing purists also flag `(const uint64_t*)key` as UB in ISO C++ (often tolerated in practice for hash functions).

**How to catch earlier:** Implement with `memcpy` into `uint64_t` chunks (compiler lowers to unaligned-safe code).

**Runtime defense:** Assert `reinterpret_cast<uintptr_t>(key) % alignof(uint64_t) == 0` only if you require alignment; otherwise fix implementation.

**Recommended fix:** Byte-wise or `memcpy`-based mixing only.

**Test coverage needed:** Hash strings at odd addresses (stack `char buf[9]` aligned +1).

---

### Finding: `ZMEYA_ASSERT` is the main internal safety rail; default handler is not in the library

**Severity:** Medium (process policy) / **Critical** if overridden to nothing  
**Category:** debug-only correctness  
**Location:** `ZmeyaConfig.h`, tests (`ZmeyaTestNewAPI.cpp`)

**Problem:** The library relies heavily on `ZMEYA_ASSERT` for invariants (TLS present, pointers in arena, patch ranges). `zm::onAssertionFailed` is **declared** in the header but **implemented only in tests**; shipping code must supply it or override the macro.

**Why it is dangerous:** Teams sometimes `#define ZMEYA_ASSERT(x) ((void)0)` in release, which **strips all internal checks** and turns many would-be asserts into raw UB on bad state.

**How to catch earlier:** Two-tier macros: `ZMEYA_HARD_ASSERT` (never stripped) for security-critical bounds, `ZMEYA_DEBUG_ASSERT` for expensive checks.

**Runtime defense:** Keep cheap checks (null TLS, span bounds) in release for deserialize entry points.

**Recommended fix:** Document forbidden macro override; provide `zm::validate_or_abort(span)` used once at asset load.

**Test coverage needed:** Build matrix proving release builds still run validation when configured.

---

### Finding: Integer width and offset limits (`goffset_t` / `roffset_t` = `int32_t`)

**Severity:** Medium  
**Category:** incorrect sizes / truncation  
**Location:** `ZmeyaTypes.h`, `ZmeyaBuilderBase.inc` (`alloc_aligned` asserts), `ZmeyaBuilderAssignImpl.inc` (`uint32_t(from.size())`), chain tables (`uint32_t` indices, `live_count_`)

**Problem:** Blobs are effectively capped near **2 GB** of addressable arena by `goffset_t` checks. Separately, `numElements` is `uint32_t`; `uint32_t(from.size())` **truncates** if a path ever fed a larger STL container (unlikely in practice, silent if it happened). Chained tables use **`uint32_t` node indices**; **`ZMEYA_HASH_CHAIN_NIL`** is `0xFFFFFFFFu`, so the representable node count must stay consistent with **`nodes.size()`** in any validator you add.

**Why it is dangerous:** Truncation or inconsistent counts yield **wrong traversal** or overlap between **live** and **free** lists in malicious blobs.

**How to catch earlier:** `static_assert` + explicit range checks before stores; validator ties `live_count_` to graph walk.

**Runtime defense:** Assert `from.size() == size_t(uint32_t(from.size()))` before stores; assert `nodes.size() < ZMEYA_HASH_CHAIN_NIL` (strictly less if `NIL` is reserved as not-a-index).

**Recommended fix:** Use `uint32_t` only after explicit range check; consider `size_t` counts with padding if format allows.

**Test coverage needed:** Boundary at `UINT32_MAX` (even if skipped by practicality); **`NIL` vs last valid index** edge cases.

---

### Finding: TLS global active builder (`thread_local` pointer)

**Severity:** Medium  
**Category:** concurrency lifetime / API design  
**Location:** `ZmeyaSerializeFoundation.h` (`g_tls_active_builder`, `ScopedBuilder`)

**Problem:** Serialize APIs resolve `detail::get_global_builder()`. Correct nesting is handled (`ScopedBuilder` restores `prev`), but **reentrancy from the same thread** with overlapping responsibilities is easy to misuse if user code calls `assign` while a different builder is expected to be active.

**Why it is dangerous:** Wrong builder leads to pointers registered against wrong arena leads to **corrupt blobs** (logic/memory corruption after load elsewhere), not necessarily immediate crash.

**How to catch earlier:** Pass `BuilderBase&` explicitly everywhere; TLS only as optional sugar.

**Runtime defense:** Debug-only TLS depth counter; assert expected builder identity on `register_roffset_slot`.

**Recommended fix:** Prefer explicit-context overloads (many already exist under `assign(builder, ...)`).

**Test coverage needed:** Nested `write_blob` / nested `ScopedBuilder` / accidental `assign` outside `write_blob`.

---

### Finding: Arena reallocation invalidates `HashMap&` / `HashSet&` (documented; custom code must mirror)

**Severity:** Medium (library-internal reduced if pattern followed) / **High** for forks or custom builder code  
**Category:** use-after-free / dangling pointers / API design  
**Location:** `ZmeyaBuilderHashChain.inc` (comments and `hm_g` / `hs_g` pattern), `AGENTS.md` (arena growth)

**Problem:** `HashMap` / `HashSet` references point into `BuilderBase::data`. **`alloc_aligned` can reallocate** the backing vector; any stale **`&hm` / `&hs`** or pointers derived before growth are **dangling** until re-bound from a stored **`goffset_t`**.

**Why it is dangerous:** Use-after-realloc is **memory corruption** in the builder or a silently wrong blob.

**How to catch earlier:** Non-copyable **handle** type instead of raw references for in-progress mutation; or pass **`goffset_t`** only.

**Runtime defense:** Debug **epoch counter** on `BuilderBase` bumped on realloc; assert current epoch when resolving offsets (optional).

**Recommended fix:** Treat `ZmeyaBuilderHashChain.inc` as the canonical pattern for any new hash or arena-mutating helpers; extend tests whenever new growth sites appear.

**Test coverage needed:** Forced small initial arena + many inserts to trigger realloc; assert outputs still match golden logical content.

---

### Finding: Iterator and `c_str` invalidation across incremental hash / string mutation

**Severity:** Medium  
**Category:** dangling references / invalidated iterators  
**Location:** `ZmeyaHashSet.h`, `ZmeyaHashMap.h` (invalidation comments), `ZmeyaString.h`, `ZmeyaSerializeApi.inc`

**Problem:** Documented behavior matches **`std::unordered_*`**: iterators and references into a set/map under mutation may invalidate. **`HashMap`** iteration now yields **references into `nodes`**; **`String::c_str()`** must not be held across **`append` / `clear`** on the same string during a write session.

**Why it is dangerous:** Classic **dangling reference / pointer** bugs in user code that caches iterators or C string pointers across **`hashmap_insert`** or string updates.

**How to catch earlier:** Type-level scoping (e.g. borrow tokens) is heavy; at minimum **documentation + examples**.

**Runtime defense:** None practical in release; ASAN on tests that deliberately cache invalid iterators.

**Recommended fix:** Keep **"use `w.root()->...` each time"** guidance prominent in ship docs.

**Test coverage needed:** Negative tests or sanitizer builds for cached `c_str` / iterator misuse patterns.

---

### Finding: `deep_copy` default path default-constructs then assigns

**Severity:** Low / Medium (depends on `T`)  
**Category:** placement-new / partial initialization  
**Location:** `ZmeyaSerializeApi.inc` (`deep_copy` templates)

**Problem:** `placementCtor<T>(p_to); *p_to = from;` assumes assignment is safe on uninitialized `T`. For blob types this is intended to be trivial.

**Why it is dangerous:** If someone deep-copies a non-trivial `T` incorrectly, you can get **double runs of invariants** or self-referential types breaking.

**How to catch earlier:** `static_assert(std::is_trivially_copyable_v<T>)` on `deep_copy` targets.

**Runtime defense:** None beyond type constraints.

**Recommended fix:** Tighten `static_assert` on `T` for blob field deep copy.

**Test coverage needed:** Types that violate assumptions (should fail compile).

---

### Finding: `BufferAllocator::allocate` does not handle allocation failure

**Severity:** Medium  
**Category:** null dereference  
**Location:** `ZmeyaSerializeFoundation.h` (`ZMEYA_ALLOC` / `allocate`)

**Problem:** POSIX path can return `nullptr`; result becomes `pointer` and `std::vector` may propagate bad state depending on STL; subsequent dereference is UB.

**Why it is dangerous:** OOM leads to **null pointer use** in growth paths.

**How to catch earlier:** Custom allocator that throws `std::bad_alloc` on null (STL expectation).

**Runtime defense:** `ZMEYA_ASSERT(pv != nullptr)` immediately after alloc macro.

**Recommended fix:** Throw or abort consistently on allocation failure.

**Test coverage needed:** Forced allocator failure injection (mock `ZMEYA_ALLOC`).

## Compile-Time Safety Improvements

- Introduce a **trusted vs untrusted** blob accessor split; make the default hardened type require a validated `BlobSpan` token.
- Add **`[[nodiscard]]`** on any future API returning validation status (several primitives already use `ZMEYA_NODISCARD`).
- **`static_assert(std::is_trivially_copyable_v<...>)`** on all types intended to live in blobs (`BuilderBase::allocate` already does for some).
- Replace raw `reinterpret_cast` root access in user code with a **`template<typename T> as_root(span)`** that checks `sizeof(T)` and alignment against header/version policy.
- Delete or poison **implicit deserialize by cast** patterns in docs; steer toward `validate_then_cast`.
- Strong typedefs: **`BlobOffset`**, **`ElementCount`**, **`ByteSize`**, **`NodeIndex`** (hash chain) to reduce multiply/add overflow mistakes (some overflow checks exist in `alloc_aligned` and slab growth).

## Runtime Debug Defenses

- Single **`validate_blob(bytes, schema_id)`** walking: every `Array` (`count`, offset, `count*sizeof(T)`), every `String` (NUL within span), every `Pointer` (target in-span, alignment), every **hash chain** (`head` / `next` in range, acyclic within step budget, `live_count_` consistent, free list disjointness as required by your format).
- **`ZMEYA_BUILDER_PARANOID`** style checks expanded beyond roffset registry (already partially present).
- Canary **magic + version** at root (application responsibility, but library can provide helpers).
- TLS **active builder stack** with mismatch detection.
- **Poison** freed/compact ranges in debug builds during arena compaction (you already compact and remap; optional fill patterns help catch stale pointer use in tests).
- Optional **realloc epoch** counter on `BuilderBase` for custom code mirroring `hm_g` / `hs_g` discipline.

## Asset / Resource Lifetime Risks

- **Mmap / file-backed blobs:** If the file shrinks or is swapped while mapped, the OS defines behavior; Zmeya does not pin or validate mapping size vs declared structure. Treat as **external asset lifetime**: validate size, then parse.
- **`c_str()` and `data()`** return pointers **into the asset**; any `std::string_view` or pointer cached after unmap is a **UAF**. Document hard: views must not outlive mapping.
- **Hot reload:** Without generation counters, code holding `T*` into the old mapping can deref after reload. Prefer **mapping handles** + explicit unmap barrier, or re-parse each frame from stable storage.
- **No unload refcounting** in the library (by design); consumers must implement pinning.

## Concurrency Lifetime Risks

- **`thread_local` builder pointer** isolates threads, but **does not** help if the same thread interleaves unrelated builders incorrectly, or if a `BlobWriter` is used after `write_blob` returns (implementation destroyed).
- **No synchronization** on read paths: concurrent read of immutable mmap is usually safe **if** the mapping is truly read-only and published safely; concurrent read during write is on the user.
- **Incremental APIs** invalidate raw pointers into the arena on growth; docs mention this; **`ZmeyaBuilderHashChain.inc`** documents the same for **hash container references**. Worth a **debug-only epoch counter** on `BuilderBase` checked when resolving cached `goffset_t`.
- **Hash iterators / `pair<const Key&, const Value&>`:** Holding iterator-derived references across **mutation** of the same container in another thread is UB unless externally synchronized (same as STL).

## Memory Stomp / Buffer Safety Risks

- **Hash chain indices** (`head`, `next`): OOB on `nodes[i]`; **cycles** cause non-termination (highest read-side change vs old bucket slices).
- **Unbounded C string scans** (`strlen`, `strcmp`).
- **Misaligned uint64 reads** in Murmur helper.
- **`reinterpret_cast` root and node downcasts** in tests (`ZmeyaTest10.cpp`) mirror what unsafe user code might do: **UB** if `nodeType` lies. In engine code, prefer `std::variant`-like dispatch tables outside the blob or tagged pointers with validation.
- **`compact_arena_and_remap_registry`**: uses `memcpy` with computed lengths; asserts partially guard; `goffset_t` mixing with `size_t` in merge logic deserves a dedicated overflow review for extreme `dead_ranges_` inputs (mostly attacker-controlled only if dead-range bookkeeping can be forced wrong).

## Test Coverage Gaps

- **Malformed blob** suite: bad offsets, bad counts, **corrupt hash chains** (OOB index, cycle, wrong `live_count_`), missing NUL, misaligned pointers.
- **OOM / null allocator** path.
- **Max size** behavior near `int32_t` / `uint32_t` limits.
- **Concurrent read** of immutable blob (sanitizer build) if you claim thread-safe read.
- **Move / reuse** of `Builder` types (already deleted copy/move on `BuilderBase`, good) plus use after `write_blob` returns negative test.
- **Pointer caching across `vector` growth** inside one `write_blob` session (`AGENTS.md` mentions; automated regression).
- **Realloc stress** for chained hash incremental paths (small initial arena, many inserts/erases/rehashes).

## Refactoring Recommendations (prioritized)

1. **Must fix immediately (if shipping untrusted blobs):** Add a **mandatory validation layer** for arrays, strings, pointers, and **chained** hash structures (indices, acyclicity, counts); eliminate unbounded `strlen` on unvalidated data; fix Murmur unaligned reads.
2. **Should fix soon:** Hard/soft assert split; allocation failure policy; explicit `uint32_t` truncation guards on all `numElements` writes; **bounded-step** traversal for defense-in-depth on corrupt chains.
3. **Nice to have:** Remove TLS-only paths in public API surface for serialize, or add a **RAII token** type proving serialize session active; debug **realloc epoch** for custom builder extensions.

## Final Verdict

**Is this code safe enough to ship?**  
**Yes, only under a narrow contract:** blobs are **trusted** (produced only by your serializer or equivalent), loaded into a **buffer at least as large as the serialized size**, and accessed **only while that storage is valid**. Under that contract, the main residual risks are **OOM handling**, **assert policy in release**, **integrator misuse** (caching raw pointers across arena growth, mmap lifetime), and **iterator / `c_str` invalidation** across incremental hash and string updates.

**What must be fixed before shipping if blobs cross a trust boundary or come from disk/network without prior authentication and integrity guarantees?**  
You **must** add **bounds-validated parsing** (or enforce that only authenticated, schema-checked packages reach the unchecked views). As written, **`HashMap` / `HashSet` / `Array::operator[]` / string hash paths are not memory-safe against arbitrary bytes`**; the hash layout change **does not remove** that requirement and **adds** chain-specific checks (indices, cycles).

**What to monitor or harden later?**  
Strict aliasing / effective-type concerns for `reinterpret_cast` blob roots (industry-standard tension for mmap serializers), allocator portability on Android macro edge cases, moving hot validation into a single cheap header checksum + lazy field checks strategy for performance, and ensuring **all** custom builder helpers follow **`hm_g` / `hs_g` rebind** rules after growth.

**Bottom line:** Zmeya is a **high-performance trusted-binary view layer**, not a hardened deserialization engine. Treat anything that casts external bytes to `TRoot*` without a prior proof pass as **shipping a memory-safety vulnerability** against malicious input.

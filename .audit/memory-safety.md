# Memory Safety Audit

Audit scope: Zmeya header-only library (`Zmeya/`): self-relative blobs, arena serialization (`ZMEYA_ENABLE_SERIALIZE_SUPPORT`), mmap-oriented read views. Assumes skeptical review of untrusted binary input.

## Executive Summary

**Overall risk level: High** for any path that treats a Zmeya blob as possibly hostile or externally supplied without a separate validation layer. **Medium** if blobs are strictly self-produced (same version, same toolchain) and never shared across trust boundaries.

Zmeya is a header-only, self-relative, mmap-friendly **view over bytes**. The read-side API (`zm::Array`, `zm::String`, `zm::Pointer`, `zm::HashSet`, `zm::HashMap`) largely **assumes layout integrity**. There is **no centralized parser** that proves offsets, counts, and bucket ranges stay inside the buffer before you dereference. That is the dominant failure class: **out-of-bounds reads and unbounded `strlen`** on malformed data, not classic heap double-free in the library itself (the serialize path uses `std::vector` and explicit bookkeeping for dead ranges).

## Highest-Risk Findings

### Finding: Read-side hash containers trust bucket indices

**Severity:** High  
**Category:** buffer overrun / asset lifetime (untrusted blob)  
**Location:** `ZmeyaHashSet.h` (`containsImpl`), `ZmeyaHashMap.h` (`findImpl`)

**Problem:** Loops use `bucket.beginIndex` / `bucket.endIndex` as slice bounds into `items` with **no check** that `beginIndex <= endIndex <= items.size()`.

**Why it is dangerous:** A corrupted or malicious blob yields **arbitrary read past the `items` slab** (information leak, crash, or ASAN fault depending on mapping).

**How to catch earlier:** A validated `ValidatedHashMapView` / `BlobCursor` type that cannot be constructed without passing checks; keep raw `HashMap` only as an internal unchecked fast path behind `unsafe_` naming.

**Runtime defense:** `ZMEYA_ASSERT` (or always-on checks) in debug; for ship, optional `validate_blob()` that walks every bucket range and every self-relative field against a `(base, size)` span before any query API runs.

**Recommended fix:** On load (or lazily on first access), verify: `buckets.size()` consistent, each `begin <= end`, `end <= items.size()`, and `sum(end-begin)` matches `items.size()` for sets (and analogous invariants for maps). Fail closed.

**Test coverage needed:** Fuzz/mutate serialized blobs: random bit flips on bucket headers, `endIndex = 0xFFFFFFFF`, `begin > end`, `end` larger than item count.

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
**Location:** `ZmeyaString.h` (`c_str`, comparisons), `ZmeyaHash.h` (`hashString` via `strlen`), incremental paths using `strlen` on arena strings (`ZmeyaBuilderBase.inc` `string_append_cstr`, `string_clear`)

**Problem:** String helpers assume a **null-terminated** payload. A truncated blob or missing `'\0'` makes `strlen` read until it walks out of the mapping.

**Why it is dangerous:** Classic **read off the end of the mmapped region**; can fault immediately or read adjacent mappings.

**How to catch earlier:** Store **length-prefixed** strings in the format, or require `StringView{ptr, len}` internal representation for blob strings.

**Runtime defense:** Every string field validated against parent span max length; binary search for terminator bounded by `(blob_end - ptr)`.

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
**Location:** `ZmeyaTypes.h`, `ZmeyaBuilderBase.inc` (`alloc_aligned` asserts), `ZmeyaBuilderAssignImpl.inc` (`uint32_t(from.size())`)

**Problem:** Blobs are effectively capped near **2 GB** of addressable arena by `goffset_t` checks. Separately, `numElements` is `uint32_t`; `uint32_t(from.size())` **truncates** if a path ever fed a larger STL container (unlikely in practice, silent if it happened).

**Why it is dangerous:** Truncation yields a **short header** pointing at a large allocation pattern or inconsistent structure if other code assumes consistency.

**How to catch earlier:** `static_assert` + `if (from.size() > max)` compile-time unreachable templates, or explicit `ZMEYA_ASSERT(from.size() <= UINT32_MAX)`.

**Runtime defense:** Assert `from.size() == size_t(uint32_t(from.size()))` before stores.

**Recommended fix:** Use `uint32_t` only after explicit range check; consider `size_t` counts with padding if format allows.

**Test coverage needed:** Boundary at `UINT32_MAX` (even if skipped by practicality).

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
- Strong typedefs: **`BlobOffset`**, **`ElementCount`**, **`ByteSize`** to reduce multiply/add overflow mistakes (some overflow checks exist in `alloc_aligned` and slab growth).

## Runtime Debug Defenses

- Single **`validate_blob(bytes, schema_id)`** walking: every `Array` (`count`, offset, `count*sizeof(T)`), every `String` (NUL within span), every `Pointer` (target in-span, alignment), every hash bucket slice.
- **`ZMEYA_BUILDER_PARANOID`** style checks expanded beyond roffset registry (already partially present).
- Canary **magic + version** at root (application responsibility, but library can provide helpers).
- TLS **active builder stack** with mismatch detection.
- **Poison** freed/compact ranges in debug builds during arena compaction (you already compact and remap; optional fill patterns help catch stale pointer use in tests).

## Asset / Resource Lifetime Risks

- **Mmap / file-backed blobs:** If the file shrinks or is swapped while mapped, the OS defines behavior; Zmeya does not pin or validate mapping size vs declared structure. Treat as **external asset lifetime**: validate size, then parse.
- **`c_str()` and `data()`** return pointers **into the asset**; any `std::string_view` or pointer cached after unmap is a **UAF**. Document hard: views must not outlive mapping.
- **Hot reload:** Without generation counters, code holding `T*` into the old mapping can deref after reload. Prefer **mapping handles** + explicit unmap barrier, or re-parse each frame from stable storage.
- **No unload refcounting** in the library (by design); consumers must implement pinning.

## Concurrency Lifetime Risks

- **`thread_local` builder pointer** isolates threads, but **does not** help if the same thread interleaves unrelated builders incorrectly, or if a `BlobWriter` is used after `write_blob` returns (implementation destroyed).
- **No synchronization** on read paths: concurrent read of immutable mmap is usually safe **if** the mapping is truly read-only and published safely; concurrent read during write is on the user.
- **Incremental APIs** invalidate raw pointers into the arena on growth; docs mention this; worth a **debug-only epoch counter** on `BuilderBase` checked when resolving cached `goffset_t`.

## Memory Stomp / Buffer Safety Risks

- **Hash bucket slices** (highest practical impact on read).
- **Unbounded C string scans** (`strlen`, `strcmp`).
- **Misaligned uint64 reads** in Murmur helper.
- **`reinterpret_cast` root and node downcasts** in tests (`ZmeyaTest10.cpp`) mirror what unsafe user code might do: **UB** if `nodeType` lies. In engine code, prefer `std::variant`-like dispatch tables outside the blob or tagged pointers with validation.
- **`compact_arena_and_remap_registry`**: uses `memcpy` with computed lengths; asserts partially guard; `goffset_t` mixing with `size_t` in merge logic deserves a dedicated overflow review for extreme `dead_ranges_` inputs (mostly attacker-controlled only if dead-range bookkeeping can be forced wrong).

## Test Coverage Gaps

- **Malformed blob** suite: bad offsets, bad counts, bad bucket ranges, missing NUL, misaligned pointers, `numBuckets == 0` with non-empty items, inconsistent sums.
- **OOM / null allocator** path.
- **Max size** behavior near `int32_t` / `uint32_t` limits.
- **Concurrent read** of immutable blob (sanitizer build) if you claim thread-safe read.
- **Move / reuse** of `Builder` types (already deleted copy/move on `BuilderBase`, good) plus use after `write_blob` returns negative test.
- **Pointer caching across `vector` growth** inside one `write_blob` session (`AGENTS.md` mentions; automated regression).

## Refactoring Recommendations (prioritized)

1. **Must fix immediately (if shipping untrusted blobs):** Add a **mandatory validation layer** for arrays, strings, pointers, and hash structures; eliminate unbounded `strlen` on unvalidated data; fix Murmur unaligned reads.
2. **Should fix soon:** Hard/soft assert split; allocation failure policy; explicit `uint32_t` truncation guards on all `numElements` writes.
3. **Nice to have:** Remove TLS-only paths in public API surface for serialize, or add a **RAII token** type proving serialize session active.

## Final Verdict

**Is this code safe enough to ship?**  
**Yes, only under a narrow contract:** blobs are **trusted** (produced only by your serializer or equivalent), loaded into a **buffer at least as large as the serialized size**, and accessed **only while that storage is valid**. Under that contract, the main residual risks are **OOM handling**, **assert policy in release**, and **integrator misuse** (caching raw pointers across arena growth, mmap lifetime).

**What must be fixed before shipping if blobs cross a trust boundary or come from disk/network without prior authentication and integrity guarantees?**  
You **must** add **bounds-validated parsing** (or enforce that only authenticated, schema-checked packages reach the unchecked views). As written, **`HashMap` / `HashSet` / `Array::operator[]` / string hash paths are not memory-safe against arbitrary bytes.**

**What to monitor or harden later?**  
Strict aliasing / effective-type concerns for `reinterpret_cast` blob roots (industry-standard tension for mmap serializers), allocator portability on Android macro edge cases, and moving hot validation into a single cheap header checksum + lazy field checks strategy for performance.

**Bottom line:** Zmeya is a **high-performance trusted-binary view layer**, not a hardened deserialization engine. Treat anything that casts external bytes to `TRoot*` without a prior proof pass as **shipping a memory-safety vulnerability** against malicious input.

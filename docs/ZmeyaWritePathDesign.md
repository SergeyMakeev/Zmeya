# Zmeya write-path evolution: design notes

This document captures a **possible future direction** for the blob writer (`zm::write_blob` and related APIs). It merges the library's existing constraints with ideas discussed for **mutable construction**, **allocator-backed building**, **finalize-time sealing**, and **binary compatibility** with today's mmap-oriented layout.

It is **not** a commitment or roadmap item by itself; it exists so trade-offs stay explicit.

---

## 1. Motivation

### 1.1 Today

The current write path is tuned for:

- A single **`write_blob`** session with thread-local **builder context**.
- **Bulk assignment** from STL-shaped values into **`zm::`** fields (`operator=` / `assign`).
- An **append-only** backing **`std::vector<char>`** (`alloc_aligned`).
- **Self-relative** pointers and arrays in the **final** blob (`Pointer`, `Array`, nested containers).

Fine-grained mutation (**push_back**, **erase**, repeated reshaping of the same logical container) is **not** offered on **`zm::`** types during the write phase; **`zm::Array`** assignment even asserts against a second full assign to reduce fragmentation and stale-offset hazards.

### 1.2 Desired ergonomics (goal)

Some users want **`zm::`** containers during construction to feel closer to **`std::vector`**, **`std::unordered_*`**, etc.: incremental operations, emplace, erase, grow and shrink, without assembling a separate STL graph and assigning once.

That ergonomics goal stresses the **current** model because:

- **Relocation** of arena memory invalidates raw addresses.
- **Self-relative** offsets stored in headers become wrong if payloads move unless **every** referring field is updated.
- **Hash tables** in Zmeya use **dense item arrays + bucket ranges**; incremental erase/insert is a different algorithmic shape than **bulk build from `std::unordered_*`**.

---

## 2. Core idea: two domains

### 2.1 Mutable construction domain (inside `write_blob`)

While the user closure runs, the implementation treats the blob as **under construction**:

- A **writer-owned allocator** backs allocations and frees (exact policy TBD: bump + finalize compaction, segregated pools, etc.).
- Indirection may use representations that are **not** valid for arbitrary mmap consumers yet: for example **arena indices**, **handle ids**, or **tagged** offset words that distinguish **build-time** vs **final** encoding.

### 2.2 Immutable mmap-ready domain (after finalize)

What leaves **`write_blob`** (or a dedicated **finalize** step) must match **today's reading contract**:

- **Trivially copyable** headers where the format expects them.
- **Self-relative** addressing for **`Pointer`**, **`Array`**, **`String`**, hash containers, etc., as documented today.
- No thread-local context required to interpret bytes.

So the coherent split:

**Mutable construction domain** (writer scope) versus **immutable mmap-ready domain** (published bytes),

with a **single sealing step** that converts from the former to the latter.

### 2.3 ABI / layout compatibility (your refinement)

You asked to **maintain both versions ABI/binary/layout compatible**. That breaks into two different tightness levels:

1. **Published blob compatibility (strong, achievable)**  
   The **final** byte sequence matches what existing readers and **`mmap`** workflows expect: same header sizes, same **`roffset_t`** semantics, same padding rules as today. Build-only tags or handles either live **outside** the serialized bytes until the last moment or are **stripped/replaced** in the sealing pass.

2. **Bitwise validity at every intermediate step (usually too strong)**  
   Demanding that **every** in-flight memory state is already a legal **read-only** blob blocks common techniques (tags, holes before compaction, uninitialized slabs). More realistic: **only the finalized buffer** is promised mmap-valid; intermediate states are **writer-private** unless explicitly documented otherwise.

Same **sizeof** / alignment for **`zm::`** headers can still hold if sealing always outputs the **same** layout types readers use today.

---

## 3. Representing pointers during build

### 3.1 Relative vs absolute (conceptual)

- **Read path (today):** **`Pointer`**, **`Array`**, etc. store **self-relative** offsets (offset from the **address of the field** holding the offset), which makes loaded blobs position-independent without fixup.

- **Write path (proposed):** While constructing, it can be easier to reason in terms of **arena-relative** indices or **allocation handles**: values stable across **append-only** growth and relocations of the **backing storage vector**, provided you key by **index into the arena**, not by raw pointer values.

### 3.2 Tagging mode without ruining performance

Several patterns coexist in the industry:

- **Reserved high bit(s)** in a word to mean "build-time handle" vs "final relative offset."
- **Low bit tagging** when **all arena allocation addresses** are known to be **aligned** (for example multiple of 2 or higher), so global byte offsets are even and bit 0 is free for a tag.

**Nuances:**

- **Arena indices** from a bump allocator that aligns starts can use **low-bit tagging** cleanly if every stored **global offset** refers to an **aligned** boundary.

- **Final self-relative deltas** are **not** automatically even: **`target - this`** can be odd (for example byte-oriented payloads). So **low-bit steal** is a better fit for **arena-global indices**, not for **final relative encoding**, unless the format also constrains alignment so that all legal deltas are even (often false for arbitrary strings).

- **Range limits:** The practical issue is not "every blob exceeds 2 GiB." It is that the **chosen integer width** for offsets (for example 32-bit vs 64-bit on disk) sets a **hard maximum** and **tag bits** reduce usable range. For many products a documented cap is fine.

---

## 4. Allocator attached to the blob writer

### 4.1 Sketch

During **`write_blob`**:

- Hold a **normal allocator** (data structure of your choice) tied to the writer: **alloc**, **free**, optional **realloc**, statistics, etc.
- Returns are **numeric**: **global byte offset** into the arena and/or **opaque handles** that the sealing pass resolves.

### 4.2 Compaction and second pass

If the construction allocator allows **free** and leaves **holes**, a **finalize / compact** step can:

- Pack live bytes into a **dense** final buffer.
- **Rewrite** every indirection from build-time representation to **final self-relative** form.

That is where a deliberate **second pass** lives: not necessarily "two user-visible passes," but **one sealing pass** after user code returns.

---

## 5. Finding every pointer site: roots, walks, registration

### 5.1 Root set

The **root type** **`TRoot`** is explicit in **`write_blob<TRoot>`**: you always know where the root object lives (today it is placed at offset 0 of the builder arena).

### 5.2 Typed graph walk vs registration

**Alternative A - Full typed walk:** From **`TRoot`**, traverse every **`zm::`** field recursively following the **static type graph**, patching offsets when objects move. Powerful but couples tightly to every container variant.

**Alternative B - Registration (your proposal):** Have each **`Pointer`** (or unified primitive) **register** its **field location** with the blob builder when constructed, so the builder owns a **complete list of pointer sites** without a separate reflection-style walk.

### 5.3 Making registration correct

- Register **stable arena coordinates**, for example **global byte offset** of the **slot** that stores the offset word (not a raw **`this`** pointer), because **`vector` reallocations move** the memory block.

- With **append-only** placement of **fixed-size headers**, existing slots' **indices** often remain stable; still define behavior if the arena policy ever **inserts** before existing objects.

- Ensure **every** code path that creates **`zm::`** headers uses **placement new** / ctors so registration runs; **raw memcpy** into the arena bypasses registration unless explicitly handled.

### 5.4 "Everything uses `Pointer<T>`"

You suggested that **`Array`**, **`String`**, **`HashSet`**, etc. could all route through **`Pointer<T>`** so only one mechanism registers indirection.

**Today in Zmeya:**

- **`String`** already carries a **`Pointer<char>`**.
- **`Array<T>`** stores **`relativeOffset`** and **`numElements`** **inline on `Array`** - it does **not** use **`Pointer<T>`** for the element slab.
- **`HashSet` / `HashMap`** nest **`Array`** headers.

So **as-is**, registration must cover **`Array`** headers (and any field holding **`relativeOffset`**) **unless** the representation is refactored.

**Refactor direction:** Introduce a **single primitive** for "offset into blob + optional extent" (name **`Span`**, **`RelPtr`**, fat **`Pointer`**, etc.) such that **strings**, **arrays**, and hash backing storage **share one implementation**. **`Pointer` alone is not enough for arrays** without a **length** somewhere - you always need **offset + extent** (explicit count, sentinel, or implicit rule).

---

## 6. Finalize: sealing and compatibility

### 6.1 Outputs

The sealing step should produce:

- A **byte-identical (to intent)** layout versus today's **`finalize`** output for the same logical data: **self-relative** fields only, **trivial** headers where required.

### 6.2 Build-only vs read types

If registration or non-trivial construction is added to **on-wire** structs, **triviality** and **mmap** guarantees may break. Common patterns:

- **Parallel metadata** keyed by arena offset (no extra fields in **`Pointer`** on disk).
- **Distinct build-time wrapper types** converted at finalize into today's **`zm::`** headers.

---

## 7. Risks and scope

| Topic | Notes |
|------|------|
| Correctness | Any relocation without updating **all** edges yields silent corruption. Registration or walks must be **complete** and **ordered** with compaction. |
| Format churn | Widening pointer words or changing hash layout breaks old blobs unless versioned. Prefer **internal** widening only until finalize. |
| Performance | Sealing is at least **O(number of edges)** plus memcpy cost; usually acceptable **once per blob**. |
| API surface | **`std`-like** erase/insert on **hash tables** implies rebuilding or a new incremental representation - orthogonal to pointer sealing. |

---

## 8. Relation to the current codebase

Today:

- **`zm::write_blob`** installs TLS, runs **`BlobWriter`**, **`finalize`** pads and copies **`std::vector<char>`**.
- **`assign`** paths allocate slabs and write **relative** metadata immediately (single-phase from the user's perspective).
- **`NEXT_STEPS.md`** documents **reallocation** hazards for captured raw pointers.

This design doc **does not** change any of that by itself; it records how a **future** writer could evolve while ** targeting** published blob compatibility.

---

## 9. Summary

1. Split **mutable construction** from **immutable mmap** behind an explicit **finalize / seal** step.

2. Use an **arena allocator** during construction; convert to **self-relative** layout at the end for **reader-compatible** bytes.

3. **Tag bits** or **parallel tables** can distinguish build-time vs final meanings; **low-bit** tagging pairs naturally with **aligned** arena offsets; **final relative deltas** need separate justification.

4. **Self-registration** of pointer sites is viable if keyed by **stable arena indices** and covers **every** indirection primitive **or** a **unified** fat-pointer / span type subsumes **`Array`** headers.

5. **ABI compatibility** is straightforward for **final output**; **bitwise** validity mid-build is usually **not** required and should not be assumed by readers.

---

## Document history

- Authoring context: discussion thread on write-path evolution, sealing, allocator-backed construction, and registration-based fixup lists.

# Zmeya write-path evolution: design specification

This document specifies the intended **mutable construction** path for the blob writer (`zm::write_blob` and related APIs): **allocator-backed building**, **finalize-time sealing**, and **binary compatibility** with today's mmap-oriented layout. It is **normative for implementation** unless a section is explicitly labeled non-normative or deferred.

---

## 0. Resolved decisions (normative)

The following choices are fixed for this design; later sections spell out consequences.

| ID | Decision |
|----|----------|
| **Q1** | **v1 scope:** Incremental **std-like** usage for **vectors and hash containers** in the first shipped version (not deferred to a later phase). |
| **Q2** | **Allocator (initial):** **Bump** allocator with **no** user-visible **free** during construction. **Temporary:** this restriction is acceptable only until the **end goal** in the next row is implemented. **End goal:** **bump + free** with holes, **mandatory compaction** at seal (see **4.2**). |
| **Q3** | **Build-time state:** Non-final indirection and bookkeeping live in **parallel metadata** keyed by stable arena identity (e.g. slot / offset), **not** in extra on-wire header fields. |
| **Q4** | **Tag bits (v1):** **No** in-place **tag bits** inside offset words that readers will interpret. Any future tagging is **not** part of v1 on-wire layout. |
| **Q5** | **Edge discovery:** **Registration** of pointer/indirection **sites** with the writer when headers are constructed or assigned (**Alternative B**), not a separate full-library typed graph walk as the sole mechanism. |
| **Q6** | **Array / string / pointer:** **One internal fat primitive** (offset + extent / count as needed); **public** names remain **`zm::Array`**, **`zm::String`**, **`zm::Pointer`**, etc., via typedefs or thin wrappers **without** forcing a single public `Pointer<T>`-only surface. |
| **Q7** | **Writer context:** **Explicit writer instance** (or handle) passed into or obtained from the API; **no** thread-local storage as the **only** way to reach builder state. |
| **Q8** | **Reallocation:** When the backing arena **relocates**, the writer **must** update **all registered slots** so stored arena-relative identities remain correct; user code must not rely on uncaptured raw pointers across growth. |
| **Q9** | **Seal ordering:** **Two-phase** finalize: (1) produce final dense byte layout / remapping; (2) patch every registered edge to **final self-relative** form. Patches must be defined so order among patches is **consistent** with this model (no conflicting assumptions). |
| **Q10** | **Compatibility:** The **final** buffer matches **today's** reader contract for the same logical data: same header layouts, **`roffset_t`** semantics, padding rules, and mmap usability as the current **`finalize`** output. |
| **Q11** | **Document role:** This file is the **authoritative** requirements description for the write-path evolution (not an exploratory note only). |

---

## 1. Motivation

### 1.1 Today (baseline codebase)

The current write path is tuned for:

- A single **`write_blob`** session with thread-local **builder context**.
- **Bulk assignment** from STL-shaped values into **`zm::`** fields (`operator=` / `assign`).
- An **append-only** backing **`std::vector<char>`** (`alloc_aligned`).
- **Self-relative** pointers and arrays in the **final** blob (`Pointer`, `Array`, nested containers).

Fine-grained mutation (**push_back**, **erase**, repeated reshaping of the same logical container) is **not** offered on **`zm::`** types during the write phase today; **`zm::Array`** assignment even asserts against a second full assign to reduce fragmentation and stale-offset hazards.

### 1.2 Target ergonomics (v1)

**Normative:** Users can use **`zm::`** containers during construction in a **std-like** way for **dynamic arrays and hash tables**: incremental insert, emplace, erase, grow and shrink, without necessarily assembling a separate STL graph and assigning once.

That goal stresses the **current** model because:

- **Relocation** of arena memory invalidates raw addresses.
- **Self-relative** offsets in headers become wrong if payloads move unless **every** referring field is updated (**Q8**).
- **Hash tables** in Zmeya use **dense item arrays + bucket ranges**; incremental erase/insert may use **rebuild** or an internal incremental strategy, but **pointer sealing and registration** remain the shared substrate.

---

## 2. Core idea: two domains

### 2.1 Mutable construction domain (inside the write session)

While user code runs inside **`write_blob`** (or the successor API that carries an **explicit writer**), the implementation treats the blob as **under construction**:

- A **writer-owned bump allocator** backs allocations (**Q2** initial policy). Opaque or numeric identities for slots are stable across arena growth when registration (**Q5**, **Q8**) is used.
- Indirection and non-final bookkeeping live in **parallel metadata** (**Q3**), not as ambiguous tags inside final header words (**Q4**).

### 2.2 Immutable mmap-ready domain (after finalize)

What leaves the writer after **finalize / seal** must match **today's reading contract** (**Q10**):

- **Trivially copyable** headers where the format expects them.
- **Self-relative** addressing for **`Pointer`**, **`Array`**, **`String`**, hash containers, etc., as documented today.
- **No** thread-local or writer context required to interpret bytes.

**Split:** **Mutable construction domain** (writer scope) versus **immutable mmap-ready domain** (published bytes), with a **single sealing step** converting the former to the latter (**Q9**).

### 2.3 ABI / layout compatibility

**Normative (final output):** The **final** byte sequence matches what existing readers and **`mmap`** workflows expect for the same logical value: same header sizes, same **`roffset_t`** semantics, same padding rules as today (**Q10**).

**Non-goal (intermediate state):** It is **not** required that every in-flight memory image be a valid read-only blob for existing readers. Only the **finalized** buffer is mmap-valid; intermediate states are **writer-private** unless explicitly documented for debugging.

**sizeof / alignment:** Published **`zm::`** header types match today's layouts after seal.

---

## 3. Representing pointers during build

### 3.1 Read path (unchanged conceptually)

**`Pointer`**, **`Array`**, etc. in a **loaded** blob store **self-relative** offsets (from the address of the field holding the offset).

### 3.2 Write path (v1)

While constructing, the writer tracks **slots** and **parallel metadata** (**Q3**, **Q5**). Reasoning uses **stable slot / arena coordinates**, not raw pointers that survive **`vector` relocation** without registration (**Q8**).

### 3.3 Tag bits (deferred, non-normative for v1)

Reserved high bits or low-bit tagging are **industry options** for a possible future where build-time state might be packed tighter. **v1 explicitly avoids** in-place tag bits in offset words readers use (**Q4**). If revisited, note: **final self-relative deltas** are not necessarily even; low-bit schemes fit **aligned arena indices**, not arbitrary final deltas.

---

## 4. Allocator attached to the blob writer

### 4.1 Initial policy (normative)

During the write session:

- Use a **bump** allocator: **alloc** returns numeric **global byte offset** and/or **handles** resolved at seal; **no** user-visible **free** in v1 (**Q2**).
- The **writer object** owns allocator state (**Q7**).

### 4.2 End goal (after initial bump-only phase)

**Target:** **Bump + free** leaving holes, then **finalize / compact** that:

- Packs live bytes into a **dense** final buffer.
- **Rewrites** every edge from build-time tracking to **final self-relative** form.

That compaction step is the natural place for the **second allocator policy** once **free** is exposed.

### 4.3 User-visible API note

Returns from allocation remain **numeric** from the user's perspective (offsets / indices into the arena); internal **opaque handles** may appear in **parallel metadata** only.

---

## 5. Finding every pointer site: roots and registration

### 5.1 Root set

The **root type** **`TRoot`** is explicit in **`write_blob<TRoot>`** (or equivalent): the root object location is known (today: offset 0 of the builder arena).

### 5.2 Mechanism (normative)

**Registration:** Each indirection primitive (**Q6** internal fat type; public **`Array`** / **`String`** / **`Pointer`**) **registers** the **location of each mutable slot** with the writer when the header is constructed or updated through supported APIs (**Q5**). A separate **full typed graph walk** is **not** the primary discovery mechanism.

### 5.3 Registration invariants

- Register **stable arena coordinates** for each **slot** (e.g. offset of the word that will hold the final relative offset), **not** a raw **`this`** pointer that becomes invalid if the arena reallocates.
- **Relocation:** On arena **reallocate**, the writer **updates** all registered records so patch targets remain correct (**Q8**).
- **Correct construction paths:** Every path that creates **`zm::`** headers must run constructors / **`assign`** so registration runs; **raw memcpy** into the arena **bypasses** registration unless explicitly documented and paired with manual registration.

### 5.4 Unified internal primitive (normative)

**One internal implementation** for "offset + extent (count)" as needed; **public** API keeps familiar names **`Array`**, **`String`**, **`Pointer`** (**Q6**). **`Pointer` alone** cannot represent variable-length arrays without a **length** field somewhere; the fat primitive carries that information internally or alongside in **parallel metadata** per **Q3**.

---

## 6. Finalize: sealing and compatibility

### 6.1 Outputs

The sealing step **must** produce a buffer that satisfies **Q10**: **self-relative** fields only, **trivial** headers where required, **byte-level** compatibility with today's **`finalize`** output for the same logical data.

### 6.2 Build metadata vs on-wire types

**Normative:** Non-final state lives in **parallel metadata** keyed by arena slot (**Q3**). On-wire **`zm::`** headers remain the **same trivial types** readers use today after seal; no extra permanent fields in **`Pointer`** on disk for registration.

### 6.3 Seal algorithm shape (normative)

**Two-phase finalize (**Q9**):**

1. **Layout phase:** Emit **dense** final bytes (including any compaction required by policy **4.2** when implemented), producing a **remap** from old arena coordinates to final positions as needed.
2. **Patch phase:** For **every** registered site, write **final self-relative** encodings. Patches must be well-defined given the remap so **implementation order** among independent sites does not violate invariants (e.g. all reads of remap complete before inconsistent mixes appear in the buffer).

---

## 7. Risks and scope

| Topic | Notes |
|------|------|
| Correctness | Missed registration or missed update on **realloc** yields silent corruption (**Q5**, **Q8**). |
| Format churn | Widening on-disk pointer words or changing hash **serialization** layout breaks old blobs unless versioned; prefer internal widening only until finalize where possible. |
| Performance | Sealing is **O(edges)** plus memcpy; expected **once per blob**. |
| Hash internals | **v1** includes std-like hash APIs; internal strategy may **rebuild** slabs or use incremental structures, but must respect **registration** and **final layout** (**Q1**, **Q10**). |

---

## 8. Relation to the current codebase

**Today:**

- **`zm::write_blob`** installs **TLS**, runs **`BlobWriter`**, **`finalize`** pads and copies **`std::vector<char>`**.
- **`assign`** paths allocate slabs and write **relative** metadata immediately (single-phase from the user's perspective).
- **`NEXT_STEPS.md`** documents **reallocation** hazards for captured raw pointers.

**Target migration (**Q7**, **Q8**):**

- Builder state lives on an **explicit writer** object (or handle), not **only** TLS.
- **Registration + writer-driven fixup** on realloc subsume ad-hoc pointer stability rules for supported APIs.

Until implementation lands, the **current** code path remains authoritative for behavior; this document specifies the **replacement** contract.

---

## 9. Summary

1. Split **mutable construction** from **immutable mmap** behind **finalize / seal** (**Q9**, **Q10**).

2. **Bump** allocator first; **bump + free + compaction** as the **end goal** (**Q2**).

3. **Parallel metadata** for build-time tracking; **no** in-place tag bits in v1 (**Q3**, **Q4**).

4. **Registration** of indirection sites; **internal fat primitive**, public **`Array` / `String` / `Pointer`** names (**Q5**, **Q6**).

5. **Explicit writer**; **automatic slot tracking** across arena relocation (**Q7**, **Q8**).

6. **Final** output is **byte-compatible** with today's readers (**Q10**).

---

## Document history

- Authoring context: discussion thread on write-path evolution, sealing, allocator-backed construction, and registration-based fixup lists.
- **Resolved decisions** (**Q1-Q11**) recorded as normative requirements for implementation.

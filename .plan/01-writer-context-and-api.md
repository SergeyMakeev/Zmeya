# 01: Writer context and public API (Q7)

## Goal

**Normative:** Builder state is reachable through an **explicit writer** passed into user code; **TLS must not be the only mechanism** for resolving **`assign`** and builder helpers (**`docs/ZmeyaWritePathDesign.md` Q7**).

## Current state (baseline)

- **`zm::write_scope<TRoot>(fn)`** in **`Zmeya/Zmeya.h`** constructs **`detail::Builder<TRoot>`**, installs **`ScopedBuilder`** which sets **`detail::g_tls_active_builder`**, passes **`BlobWriter<TRoot>`** to **`fn`**, then **`finalize`**.
- **`assign(...)`** and **`deep_copy`** paths call **`detail::get_global_builder()`** and **`assert`** non-null.

## Target state

- User-facing closure receives **`BlobWriter<TRoot>`** (or renamed equivalent) that **carries** or **references** the active **`BuilderBase`** without requiring TLS for correctness.
- **`assign`** implementations obtain **`BuilderBase*`** from a parameter or from a writer-scoped facility that is **not** thread-global-only.

## Implementation steps

### Step A: Thread context refactors (internal)

1. Introduce **`detail::ActiveWriter`** or extend **`BlobWriter`** to hold **`BuilderBase*`** (already **`impl_`**).
2. Add **`BuilderBase* BlobWriter::builder_base() const`** (or package **`ScopedBuilder` + writer** as one RAII object) for internal use.

### Step B: Plumb builder pointer into assign paths

Preferred direction (minimize churn):

1. Add overloads or internal helpers **`assign_impl(BuilderBase&, ...)`** used by **`assign`** after resolving builder.
2. **`get_global_builder()`** becomes a **fallback** used only when **`assign_impl`** is called without an explicit builder (remove after migration), **or** remove TLS entirely once all call sites pass builder.

**Preferred long-term (matches Q7 strictly):**

- **`assign`** functions take **`BuilderBase&`** as first parameter **or** take **`BlobWriter&`** and extract implementation pointer.
- **`operator=`** on **`zm::`** types cannot pass **`BlobWriter`** without storing a back-pointer on each header (rejected: bloated on-wire layout). So **`operator=`** continues to use **TLS or a function-scope builder** unless you introduce a **per-thread current writer** set only inside **`write_scope`** (still TLS, but documented as **implementation detail of the entrypoint**).

**Clarification for implementers:** Q7 forbids **TLS as the only way** - meaning tests and alternate entrypoints must be able to call **`assign`** with an explicit **`BuilderBase`**. The **`write_scope`** convenience wrapper may still set TLS for **`operator=`** ergonomics if **`operator=`** remains zero-extra-storage.

### Step C: Public API surface

1. Document that **`fn(BlobWriter)`** is the supported handle; **`root()`**, **`allocate()`**, **`contains_pointer()`** stay stable.
2. Optional: **`write_scope`** overload **`write_scope(fn, writer_options)`** with explicit initial size / alignment only (no API creep).

### Step D: Deprecation / cleanup

1. Mark **`get_global_builder()`** **`detail`** only; grep tests for reliance on TLS from nested threads (must remain unsupported or explicitly set writer).
2. Update **`NEXT_STEPS.md`** threading section once explicit **`assign`** overloads exist.

## Deliverables

- Refactored **`assign`** / **`deep_copy`** resolution paths with **tests** calling **`assign(builder, ...)`** without **`ScopedBuilder`** on TLS (where **`BuilderBase`** is stack-owned test double - may require **`friend`** or test-only accessor).

## Risks

- **`operator=`** on **`zm::String`**, **`zm::Array`**, etc. widely used; breaking signature without overload set hurts UX. Mitigation: keep **`operator=`** -> TLS path **plus** add **`zm::assign(writer, lhs, rhs)`** free functions as the explicit API.

## Dependencies

- None (foundational). **02-registration** should use **`BuilderBase*`** passed from writer, not global getter where avoidable.

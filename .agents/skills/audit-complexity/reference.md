# audit-complexity rubric (reference)

Use this checklist while auditing. Not every item applies to every repo.

## 1. Readability and cognitive load

- Logic spread across many files or layers; overuse of callbacks, inheritance, metaprogramming.
- Behavior hidden behind generic helpers; names that do not express intent.
- Stale or misleading comments; control flow hard to follow; unrelated responsibilities mixed.

For each issue: why load is high, what context the reader must hold, how to simplify.

## 2. Dead and unused code

Unused functions, types, files, constants, flags, parameters, unreachable branches, test-only leakage in production paths.

Classify: safe to delete / probably safe (verify) / suspicious, not safe yet.

## 3. Multiple ways to do the same thing

Competing patterns (error handling, config, initialization, serialization paths, naming). Prefer one; document migration.

## 4. Useless helpers

Wrappers that only forward with no invariant, validation, or clarity. Ask: easier to read if inlined?

## 5. Large functions and files

Rough guide: functions over ~50 lines deserve review; over ~100 often should split. Flag mixed responsibilities (validation + IO + business logic in one place).

## 6. Cyclomatic complexity

Nested branches, long else-if chains, complex booleans, implicit state machines. Prefer guards, early returns, smaller functions.

## 7. Non-linear execution flow

Callbacks, hidden registration, globals, surprising side effects in constructors/getters, async without clear ownership.

## 8. Naming

Generic names (Handler, Manager, Util), misleading names, inconsistent conventions.

## 9. File and module structure

Huge grab-bag modules, weak boundaries, public API too wide, circular includes.

## 10. Duplication

Copy-paste and near-duplicate logic. Decide: keep for clarity, shared helper, delete one path, or simplify design.

## 11. Hidden state and side effects

Thread-local, singletons, static mutable, caches, APIs that look pure but mutate.

## 12. Tests and debuggability

Coupling to IO/time/randomness/globals; weak asserts; tests that mirror implementation instead of behavior.

## Audit tone

Be direct. Do not be polite at the expense of clarity. The deliverable is the markdown file plus a brief pointer in chat.

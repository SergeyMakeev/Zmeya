---
name: audit-complexity
description: >-
  Runs a senior-style maintainability and complexity audit (readability, cognitive load,
  duplication, dead code, TLS/side effects, file size). Writes the report to a markdown file
  under `.audit/`. Use when the user asks for a complexity audit, maintainability review,
  cognitive-load review, or invokes the skill name `audit-complexity`.
disable-model-invocation: true
---

# audit-complexity

## Goal

Produce a **direct, critical** audit focused on making the codebase **smaller, simpler, and easier to change** for mid-level maintainers. Prefer deletion and explicit code over clever abstractions.

## Output file

1. **Default path:** `.audit/code-complexity.md`
2. If the user gives a path or filename, use that instead (still prefer under `.audit/`).
3. Ensure the `.audit/` directory exists (create it if missing).
4. **Overwrite** the default file when re-running unless the user asks for a dated copy; if they want history, use `.audit/code-complexity-YYYY-MM-DD.md`.

## Workflow

1. **Scope:** Confirm whether the audit is repo-wide or limited paths; default is the project source (exclude `extern/`, vendored trees, and generated output unless asked).
2. **Explore:** Scan layout, largest files, serialization/build hot paths, tests. Use search and reads; do not guess file contents.
3. **Apply rubric:** Work through the checklist in [reference.md](reference.md). Every finding needs severity, location, problem, why it matters, recommendation, confidence.
4. **Dead code:** Grep for unused symbols, macros, and one-off helpers; classify safe vs needs verification.
5. **Write:** Emit the full report in markdown using the template below. Use ASCII-only text in the file.
6. **Summarize in chat:** One short paragraph pointing to the written file path.

## Report template

Use this structure in the output `.md` file:

```markdown
# <Project or scope> code complexity and maintainability audit

## Executive Summary
(Top risks; list the top 5 fixes first.)

## Severity Legend
(Critical / High / Medium / Low definitions.)

## Findings
### Finding: <title>
**Severity:** ...
**Location:** File, symbol, approximate lines
**Problem:** ...
**Why it matters:** ...
**Recommendation:** ...
**Confidence:** High / Medium / Low

(repeat per finding)

## Dead Code Candidates
(table or list: location, why unused, safe to delete?, verification)

## Complexity Hotspots
(table: hotspot, why complex, simplification)

## Duplicate Patterns / Multiple Ways To Do The Same Thing
(pattern A vs B, which to keep, migrate/delete)

## Useless Abstractions / Wrappers
(location, verdict: keep / inline / delete)

## Large Function / Large File Review
(location, size, mixed responsibilities, split suggestion)

## Recommended Refactor Plan
1. Safe deletions
2. Low-risk simplifications
3. Medium-risk refactors
4. Larger architectural cleanups

## What Not To Change
(unusual code that is justified; explain why)
```

## Principles (short)

- Prefer **simple, explicit, linear** code; flag cleverness, hidden TLS/globals, and magic dispatch.
- Prefer **deletion** over refactor when unused.
- Prefer **inlining** over one-line wrappers that hide behavior.
- Do **not** suggest new abstractions unless they clearly reduce complexity.
- For Zmeya specifically, read `AGENTS.md` for build/write-path constraints before recommending API breaks.

## Additional rubric detail

For the full audit dimensions and examples, read [reference.md](reference.md) when producing the report.

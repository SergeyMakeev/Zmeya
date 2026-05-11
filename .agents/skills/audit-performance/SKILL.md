---
name: audit-performance
description: >-
  Runs a performance-oriented audit (hot paths, algorithmic cost, allocations, copies,
  cache locality, serialization/finalize costs). Writes the report to a markdown file
  under `.audit/`. Use when the user asks for a performance audit, perf review, hot-path
  analysis, complexity/alloc audit, or invokes the skill name `audit-performance`.
disable-model-invocation: true
---

# audit-performance

## Goal

Produce a **direct, evidence-based** performance audit: where time and memory go, what scales badly, and what is **mitigated vs still open** after recent changes. Prefer **file + symbol** citations and **complexity class** language (amortized / Theta / worst case) over vague slowness claims.

## Output file

1. **Default path:** `.audit/performance-audit.md`
2. If the user gives a path or filename, use that instead (still prefer under `.audit/`).
3. Ensure the `.audit/` directory exists (create it if missing).
4. **Overwrite** the default file when re-running unless the user asks for a dated copy; if they want history, use `.audit/performance-audit-YYYY-MM-DD.md`.
5. Use **ASCII-only** text in the markdown file (no Unicode symbols or smart punctuation).

## Workflow

1. **Scope:** Repo-wide or paths the user names; default is library and test sources. Exclude `extern/`, vendored trees, and build output unless asked.
2. **Baseline:** Record current **`git rev-parse HEAD`** and short **`git log -1 --oneline`** in a **Last synced to implementation** line near the top of the report.
3. **Explore:** Map hot paths with search and file reads (`write_blob`, `finalize`, incremental APIs, containers, read-side `find` / iteration). Do **not** invent behavior; cite code.
4. **Apply rubric:** Work through [reference.md](reference.md). Classify each issue: **Critical / Major / Minor** with **status** (open, mitigated, by design, needs measurement).
5. **Tables:** Include at least an **issue table** (issue, location, status) and a **complexity reference** where it helps (before vs after if the user is mid-refactor).
6. **Write:** Emit the full report using the template below.
7. **Summarize in chat:** One short paragraph with the output path.

## Report template

Use this structure in the output `.md` file (adapt section titles if the project is not Zmeya):

```markdown
# <Project or scope> performance audit

**Last synced to implementation:** commit `<full or short hash>` (*<subject>*, <YYYY-MM-DD>).

## 1. Executive summary
(Bullet list: read path, write path, biggest cliffs, what is still Theta(n) per op.)

## 2. Scope and hot paths
(What workloads matter: serialization session, finalize, mmap read, etc.)

## 3. Implementation status (optional)
(If auditing mid-change: table of landed vs open work, key commits.)

## 4. Findings

### 4.1 Overall health
(Paragraph on bulk vs incremental paths, average vs worst case.)

### 4.2 Issue table
| Issue | Location | Status |
|-------|----------|--------|
| ... | file / symbol | open / mitigated / ... |

### 4.3 Hotspot details
(Per area: layout, complexity, locality, allocator behavior.)

### 4.4 Costs you cannot fix in one line
(finalize registry, optional API cliffs — be explicit.)

## 5. Open work and design options
(Ordered list: benchmarks, API changes, format changes — mark risk.)

## 6. Top practical fixes
(Numbered, actionable, tied to the issue table.)

## 7. File references
| Topic | Files |
|-------|-------|
| ... | ... |

## 8. Conclusion
(Short; what to measure next if uncertain.)
```

## Project note (Zmeya)

Before recommending API or wire-format changes, read **`AGENTS.md`** for build flags, `write_blob` / TLS behavior, and incremental constraints.

## Additional rubric

For dimensions, checklists, and example finding wording, read [reference.md](reference.md) when producing the report.

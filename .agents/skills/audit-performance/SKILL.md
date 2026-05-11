---
name: audit-performance
description: >-
  Runs a performance-oriented audit (hot paths, algorithmic cost, allocations, copies,
  cache locality, serialization/finalize costs). Uses code plus real benchmark numbers
  when available; records benchmark gaps and proposed tests in the report. Writes the
  report to a markdown file under `.audit/`. Use when the user asks for a performance
  audit, perf review, hot-path analysis, complexity/alloc audit, or invokes the skill name
  `audit-performance`.
disable-model-invocation: true
---

# audit-performance

## Goal

Produce a **direct, evidence-based** performance audit: where time and memory go, what scales badly, and what is **mitigated vs still open** after recent changes. Prefer **file + symbol** citations and **complexity class** language (amortized / Theta / worst case) over vague slowness claims.

**Measurements over guesses:** Treat **perf numbers** (microbenchmarks, profiles, wall time in controlled runs) as the authority when they exist. **Do not** phrase unmeasured hypotheses as settled facts. When the repo has a benchmark harness (e.g. Zmeya **`ZmeyaBench`** / **`run_perf_tests.cmd`**), the audit should either reference recent results or state that none were run and **list concrete gaps**: what to add or extend in the benchmark suite, written so an implementer can drop them into the harness. Those **benchmark gaps and proposed tests** belong **in the audit report** (dedicated section), not only in chat.

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
4. **Measurements:** If the project ships benchmarks (Zmeya: **`ZmeyaBench`**, enable with **`ZMEYA_BUILD_BENCHMARKS`**, run **`run_perf_tests.cmd`** or equivalent), incorporate **actual numbers** when available (paste summary table or path to saved output). Never substitute invented timings or **x%** claims. If a hot path has **no** benchmark yet, mark the finding **needs measurement** in the issue table and add an explicit line in **Benchmark coverage and gaps** (what to build, input shape, what to compare).
5. **Apply rubric:** Work through [reference.md](reference.md). Classify each issue: **Critical / Major / Minor** with **status** (open, mitigated, by design, needs measurement).
6. **Tables:** Include at least an **issue table** (issue, location, status) and a **complexity reference** where it helps (before vs after if the user is mid-refactor).
7. **Write:** Emit the full report using the template below (including **Benchmark coverage and gaps**).
8. **Summarize in chat:** One short paragraph with the output path.

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

## 9. Benchmark coverage and gaps
(Summarize what was measured with **real numbers** vs what was inferred from code only. If nothing was run, say so plainly. List **missing** benchmarks as **proposed tests** concrete enough to add to the harness, e.g. new `BENCHMARK` cases in **`ZmeyaBench.cpp`**, workload shape, and success criteria. Do not guess timings here.)
```

## Project note (Zmeya)

Before recommending API or wire-format changes, read **`AGENTS.md`** for build flags, `write_blob` / TLS behavior, and incremental constraints. For numbers, prefer **`ZmeyaBench`** (**`-DZMEYA_BUILD_BENCHMARKS=ON`**) and **`run_perf_tests.cmd`**; extend **`ZmeyaBench.cpp`** when the audit identifies coverage gaps, and record those proposals in **section 9** of the report.

## Additional rubric

For dimensions, checklists, and example finding wording, read [reference.md](reference.md) when producing the report.

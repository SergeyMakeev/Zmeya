---
name: audit-memory-safety
description: >-
  Performs a skeptical C++ memory-safety audit (ownership, UAF, buffers, deserialization,
  concurrency, assets) and writes a structured Markdown report. Use when the user asks for a
  memory safety audit, security review of native code, audit of allocators/parsers/blobs, or
  invokes the audit-memory-safety skill by name.
disable-model-invocation: true
---

# audit-memory-safety

## When to apply

Use this skill when the user wants a **memory safety audit** of C/C++ (or mixed) codebases: manual memory, arenas, serialization, mmap blobs, intrusive containers, threads, etc. The agent must **inspect the repository** (read code, grep for risky patterns, check recent `git` commits when relevant) rather than answering from memory alone.

## Output artifact (required)

1. Ensure the directory **`.audit/`** exists at the **repository root** (create it if missing).
2. Write the full audit to a Markdown file:
   - **Default path:** `.audit/memory-safety.md`
   - **If the file already exists** and the user did not ask to overwrite: use **`.audit/memory-safety-YYYY-MM-DD.md`** (use the date from the user environment / conversation), or a path the user explicitly gives.
3. Use **ASCII-only** in the generated file unless the user requests otherwise.

## Workflow

1. **Scope:** Confirm target (whole repo, `Zmeya/`, specific paths). If unclear, default to the workspace root excluding obvious third-party trees (`extern/`, `vendored/`, `node_modules/`, etc.) unless the user wants them included.
2. **Baseline:** Run `git log -1 --oneline` and `git show --stat HEAD` (or equivalent). If the latest commit touches safety-relevant code, add an **Audit revision** subsection citing the commit hash and summarizing impact on the audit.
3. **Investigation:** Search and read for at least:
   - `reinterpret_cast`, `memcpy`, `memmove`, `memset`, placement `new`, manual `new`/`delete`, `malloc`/`free`
   - Raw pointers stored in containers; pointers into `vector`/`string` that may reallocate
   - `thread_local`, `async`, mutexes, captured `this` / raw pointers in lambdas
   - Deserialize / mmap / offset arithmetic without bounds checks
   - Assert macros (`assert`, `ZMEYA_ASSERT`, stripped-in-release patterns)
4. **Severity:** Be direct. Untrusted binary input without validation is almost never "Low" risk.
5. **Deliverable:** Populate the report using the **Report template** below (same section order). Save to the output path, then tell the user the path in one line.

## Report template

Copy this structure into the output `.md` file. Replace bracketed hints with findings; omit empty sections only if truly N/A.

```markdown
# Memory Safety Audit

Audit scope: [paths / components]

## Audit revision (optional)

[If git baseline reviewed: commit hash, one-line subject, bullet list of code changes that affect the audit.]

## Executive Summary

Overall risk level: [Low / Medium / High / Critical] for [trusted-only vs untrusted input contract].

[2-4 sentences: dominant failure modes, what the code assumes.]

## Highest-Risk Findings

[For each finding, use:]

### Finding: [short name]

**Severity:** Critical / High / Medium / Low  
**Category:** [use-after-free / use-after-move / leak / memory stomp / ownership ambiguity / concurrency lifetime / asset lifetime / API design / other]  
**Location:** [file / function / class]  
**Problem:**  
**Why it is dangerous:**  
**How to catch earlier:**  
**Runtime defense:**  
**Recommended fix:**  
**Test coverage needed:**  

---

[Repeat for additional findings, most dangerous first.]

## Compile-Time Safety Improvements

[Bullets: types, ownership, deleted ops, nodiscard, static_assert, etc.]

## Runtime Debug Defenses

[Bullets: asserts, validators, bounded walks, canaries, etc.]

## Asset / Resource Lifetime Risks

[If applicable; else short "N/A for this scope".]

## Concurrency Lifetime Risks

[If applicable.]

## Memory Stomp / Buffer Safety Risks

[Pointer arithmetic, memcpy, parsers, alignment, strict aliasing.]

## Test Coverage Gaps

[Concrete test ideas, fuzz, negative paths.]

## Refactoring Recommendations

1. Must fix immediately: [...]
2. Should fix soon: [...]
3. Nice to have: [...]

## Final Verdict

- Safe enough to ship under what contract?
- What must be fixed before shipping untrusted / external input?
- What to monitor or harden later?

**Bottom line:** [One blunt sentence.]
```

## Quality bar

- Prefer **evidence-backed** findings (cite paths, symbols, or behavior).
- Do **not** treat sanitizers as the primary strategy; mention them only after design-level mitigations.
- Match the project's **comment and encoding rules** if the repo has `.cursor/rules` (e.g. ASCII-only code files); the audit Markdown is still ASCII unless the user opts in.

## Reference

For a filled example aligned with this repository, see [.audit/memory-safety.md](../../../.audit/memory-safety.md) when it exists.

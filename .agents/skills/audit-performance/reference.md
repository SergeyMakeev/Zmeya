# audit-performance reference rubric

Use this when filling in the performance audit report. Every important claim should tie to **observed code** or **stated uncertainty** (e.g. "needs benchmark"). **Do not treat unmeasured performance as fact:** prefer real numbers from the project's benchmark harness; if none exist, say so and propose concrete tests.

## Dimensions to cover

1. **Algorithmic complexity**
   - Per-operation: insert / erase / find / clear / iteration for each major container API.
   - Distinguish **average** (hashing assumptions) vs **worst case** (long chains, pathological keys).
   - Flag **Theta(n) per call** snapshots, full rebuilds, or linear scans that should be amortized O(1) or O(log n).

2. **Allocations and temporaries**
   - Heap allocs inside loops (e.g. `std::string` per element for comparison or rebuild).
   - `vector` growth, rehash auxiliary buffers, per-rehash `std::vector` scratch.
   - Snapshot paths that build STL maps/sets from blob views.

3. **Copies and moves**
   - Mandatory copies at API boundaries (e.g. return value from finalize, span-to-vector).
   - Shallow vs deep copy mistakes on iteration (iterators returning heavy values by value).

4. **Memory locality and layout**
   - Chained vs flat storage: full-table scan cache behavior.
   - SoA vs AoS where relevant.

5. **Write path session costs**
   - Arena growth / realloc invalidation (document for users).
   - Compaction: dead ranges, registry size, patch pass cost in big-O terms when obvious.

6. **Read path**
   - mmap vs copy-in; alignment; pointer chasing.

7. **Correctness vs performance tradeoffs**
   - Call out code that looks slow but is required for safety (e.g. pointer refresh after realloc).

## Severity labels (suggested)

- **Critical:** Dominates runtime or memory for realistic workloads; grows with n per interactive op.
- **Major:** Significant on hot paths or large payloads; fix or workaround clearly valuable.
- **Minor:** Constant factors, rare branches, or micro-opts; note only if cheap to fix.

## Status column vocabulary

- **Open** — not addressed.
- **Mitigated** — improved for a subset of types or cases; say which.
- **Unchanged / by design** — documented limitation or intentional cost.
- **Needs measurement** — hypothesis only; suggest benchmark shape.

## Evidence rules

- Prefer **path + API name** over line numbers (line numbers drift).
- When comparing before/after a refactor, use **commit hashes** from `git log`.
- If the codebase has trait gates (`if constexpr`, type traits), mention **which types** take fast vs slow paths.

## Benchmarks section

- **Authority:** Measured latency, throughput, or allocation counts from a controlled run beat intuition. Cite a table snippet, log path, or state that benches were not run this session; never invent **x%** or nanoseconds.
- **Gaps in the report:** When coverage is missing, the audit **must** include a **Benchmark coverage and gaps** section (skill template section 9): list each missing scenario as a **proposed** `BENCHMARK` (or profiler scenario): name, inputs, what to compare, and how to know it passed (e.g. logical equality after deserialize).
- **Zmeya:** Prefer **`ZmeyaBench`** / **`run_perf_tests.cmd`**; extend **`ZmeyaBench.cpp`** for new cases called out in the audit.
- If no numbers exist yet, keep a short **Suggested benchmarks** subsection under gaps: input sizes, what to compare (logical equivalence vs byte identity), Release vs Debug caveats, and **`--benchmark_repetitions`** for noisy cases.

## What not to do

- Do not claim a specific **x% speedup** or absolute timing without measurements.
- Do not recommend breaking wire format without labeling it as a **compatibility** decision.
- Avoid non-ASCII characters in the generated audit file.

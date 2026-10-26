# ADR-003: Memory and safety model

Status: proposed (one-way-door checkpoint — user decision pending)
Date: 2026-10-26
Question: Manual management, tracing GC, ARC, ownership, or a hybrid? What is
the unsafe story?

Options considered (at least two):

  A. Tracing GC runtime (JVM/.NET/Go style). Evidence: C-613 (pause latency is
     engineerable, but vendor-claimed numbers), C-211/C-212 (generational
     barrier design costs). Costs: a runtime we must ship and tune; conflicts
     with AOT-native minimal-runtime direction; pause-risk is a recorded
     conflict (#9).
  B. ARC (Swift style). Evidence: C-608 (no tracing pauses, but reference
     cycles leak unless weak/unowned are used — manual work re-enters).
  C. Manual management only (C style). Evidence: C-104 (manual management
     dominates serious CVEs across vendors with different denominators) —
     this is the null hypothesis we are trying to beat.
  D. Ownership + borrowing with an explicit unsafe boundary; arenas/regions
     as a library, not runtime magic; NO GC in v0.1.0. Evidence: C-601/C-602
     (ownership is formally proven (RustBelt) and adopted in industry),
     C-603 (residual bugs concentrate in unsafe — argues for a small,
     auditable unsafe boundary), C-112 (learning cost is real and measurable
     — honesty requirement), C-617 (Send/Sync-style static race prevention
     works), C-624 (region/arena hypothesis), C-623 (hybrid hypothesis).

Trade-offs: D buys compile-time UAF/double-free/data-race elimination in the
safe subset (the single largest CVE class, C-104..C-106) without a runtime,
at a documented ergonomic/learning cost (C-112) and real checker
implementation cost (NLL took Rust a major redesign, C-620). A and B push
costs into runtime complexity or cycle management; C forfeits the safety
goal in ADR-001. D's learning-cost risk is mitigated by diagnostics
investment (C-313 lesson) but is not eliminated — see falsification metric.

Decision and reasoning (RECOMMENDATION to user — not yet final): Option D:
- Safe subset by default: no use-after-free, no double-free, no data races
  by construction; ownership + borrows, minimal rules over maximal
  expressiveness for v0.x (fewer borrow features than Rust, explicitly).
- Explicit `unsafe` block/boundary from v1, kept small and auditable;
  every unsafe escape documented in an illegal-behaviour catalogue
  (anti-pattern is C-101's open-ended UB; model is Zig's checked illegal
  behaviour philosophy where feasible).
- Arenas/regions as library types (no runtime GC); no GC in v0.1.0; adding a
  GC later would be a new execution-mode decision, not a silent addition.
- Races: never UB — prevented in safe code, and a dynamic detector is the
  planned backstop (C-617/C-618 lesson from Go's detector-only gap, C-619).

Reversibility (cheap / costly / one-way): one-way in practice — it defines
what every library and the stdlib look like, and public code outlives the
checker (Java/C# nullability retrofits took a decade, C-215..C-218).

What result would show this was wrong (experiment or metric): (1) Phase 3
borrow-checker ergonomics probe on 3 realistic small programs (the deferred
E2 — our own checker) showing friction so high that simple programs need
unsafe escapes routinely (threshold: >30% of probe programs need unsafe for
non-performance reasons); (2) checker complexity exploding beyond the
v0.1.0 scope budget (ADR-005) — then fall back to "memory-safe via runtime
checks" or revisit Option A, and record the pivot in the decision log.

Evidence IDs: C-101, C-104, C-105, C-106, C-112, C-211, C-212, C-215, C-216,
C-217, C-218, C-601, C-602, C-603, C-608, C-613, C-617, C-618, C-619, C-620,
C-623, C-624

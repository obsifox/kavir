# ADR-002: Implementation strategy — backend, IR, execution model, compiler language

Status: proposed (one-way-door checkpoint — user decision pending)
Date: 2026-10-26
Question: Reuse an existing backend/IR or build one? VM or native? What is
the compiler written in?

Options considered (at least two):

  A. LLVM as the only backend from day one. Evidence: C-402 (permissive
     license, mature, target breadth). Costs: heavyweight dependency, slow
     codegen vs light backends (C-405), version churn (C-403, C-409),
     bootstrap weight (C-421).
  B. Managed VM (JVM/.NET-style) or interpreter-only for v0.1.0. Evidence:
     C-201/C-203 (VM hosting is language-independent and capable). Costs:
     JIT warmup/startup officially admitted (C-204, C-205); AOT escape hatches
     pay closed-world costs (C-207, C-210); ties Kavir to a foreign runtime.
  C. Custom optimizing backend from scratch. Evidence: C-419/C-420 (Go shows
     it works — with institutional funding and years of effort).
  D. QBE-first native AOT behind a thin language IR, with a documented path
     to add LLVM or Cranelift as a second tier later. Evidence: C-413 (QBE:
     70%-of-performance goal in 10% of the code), C-405 (light backends
     compile an order of magnitude faster, code somewhat slower), C-414/C-415
     (language-level IR under a reused backend is the proven pattern), C-425
     (two-tier hypothesis), plus our own E1 probe (QBE delivered 57% of the
     gcc -O0→-O2 run-time improvement at lower compile time on the probe
     workload — benchmarks/e1-backend-probe.md).

Trade-offs: A buys target breadth and peak codegen immediately at dependency
and compile-speed cost; B buys a runtime we do not control; C is not credible
for a small team (Go's own history); D buys a working native toolchain fast
and keeps the door open, at the cost of accepting lower peak performance in
v0.x and per-target ABI work regardless of backend (C-501..C-506).

Decision and reasoning (RECOMMENDATION to user — not yet final): Option D,
with these sub-decisions:
- Execution model: AOT-native binaries via QBE → cc for linking; no managed
  runtime; no GC runtime dependency (ties to ADR-003).
- IR: one SSA-based language IR (Kavir IR, typed, small) from which QBE IL is
  emitted; later tiers compile from the same IR (C-414/C-415 pattern).
- Compiler implementation language: C17. Reasons: the sandbox where the
  compiler is developed and tested has gcc but no Rust/Go/Zig toolchain
  (recorded 2026-10-26); QBE itself is C; a C compiler with no runtime
  dependency matches the AOT-native story and the freestanding ambitions
  (C-516); Zig's bootstrap path shows C as a portable substrate (C-421).
  Self-hosting remains a later possibility, not a v0.1.0 goal.
- Compiler-source license note: QBE is MIT; linking strategy is cc-based
  (no QBE library dependency), so Kavir emits QBE IL text.

Reversibility (cheap / costly / one-way): the compiler-language choice is
costly to reverse mid-project but not one-way. The execution model (native
AOT vs VM) and the IR shape are effectively one-way once public programs and
ABI exist — hence this checkpoint. Adding a second backend tier later is
additive by design.

What result would show this was wrong (experiment or metric): (1) E1-style
probe scaled to Phase 3 workloads showing QBE's runtime gap vs an optimizing
tier >10x on representative code (would justify LLVM tier earlier); (2)
 inability to express Kavir's calling conventions/aggregate passing in QBE IL
(FFI probe E5 showed the scalar/pointer path works); (3) per-target needs
(QBE supports amd64/arm64/rv64 only) that a v0.x tier-1 list does not need
beyond those.

Evidence IDs: C-201, C-203, C-204, C-205, C-207, C-210, C-402, C-403, C-405,
C-409, C-413, C-414, C-415, C-419, C-420, C-421, C-425, C-501, C-506, C-516
Local artifacts: benchmarks/e1-backend-probe.md, benchmarks/e5-ffi-probe.md

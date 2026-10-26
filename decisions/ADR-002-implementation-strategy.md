# ADR-002: Implementation strategy — backend, IR, execution model, compiler language

Status: accepted (user decision 2026-10-26 — the original QBE-first
recommendation was REJECTED by the user and this ADR was REWRITTEN with the
user's binding constraints)
Date: 2026-10-26 (v2, rewritten same day after user review)
Question: Reuse an existing backend/IR or build one? VM or native? What is
the compiler written in?

User constraint (binding, 2026-10-26, chat): custom backend generating
assembly/machine code directly; NO C path and no cc anywhere in the
compiled-program path; no LLVM/Cranelift/QBE; Linux x86-64 only at first;
thin IR; no libc (direct syscalls only); designed so that a second backend
can be added later.

Options considered:

  A. LLVM as the only backend from day one. Evidence: C-402 (permissive
     license, mature, target breadth). REJECTED by the user constraint, and
     independently on cost grounds: heavyweight dependency, compile-speed
     and version-churn costs (C-403, C-405, C-409), bootstrap weight
     (C-421).
  B. Managed VM (JVM/.NET-style) or interpreter-only for v0.1.0. REJECTED:
     foreign runtime we do not control; JIT warmup/startup officially
     admitted (C-204, C-205); AOT escape hatches pay closed-world costs
     (C-207, C-210).
  C. Light third-party backend (QBE) emitting IL text and letting cc
     assemble and link — the ORIGINAL recommendation of this ADR. REJECTED
     by the user: every produced binary would pass through cc (a C
     toolchain path) and through an external backend, both excluded by the
     constraint (context: C-413, C-405).
  D. Custom backend emitting assembly text, assembled and linked by GNU
     binutils (as/ld — no cc). Considered as the nearest non-cc fallback;
     REJECTED for v0.1.0 because it still depends on external toolchain
     components and their target defaults (C-517), which the intent of the
     constraint rules out. Retained ONLY as an explicit fallback requiring
     a fresh user decision if the ELF/ABI work stalls (falsification gate
     2 below) — never a silent fallback.
  E. ACCEPTED: custom backend with direct x86-64 machine-code emission,
     internal ELF writer, thin language IR, no libc. Precedent: Go built
     and maintains its own SSA backend for compile-time and control reasons
     (C-419, C-420) — Go's history is simultaneously the caution (years of
     dedicated-team effort); a thin, target-neutral language IR feeding
     separate lowering tiers is the proven architecture (Swift SIL C-414,
     rustc MIR C-415); the two-tier strategy remains a recorded hypothesis
     (C-425).

Decision (ACCEPTED — user-imposed constraints):

- Execution model: AOT-native static ELF executables (ET_EXEC, non-PIE in
  v0.1.0). No managed runtime, no GC runtime (ties to ADR-003), no libc,
  no dynamic linker/interpreter, no external linker.
- Backend: Kavir's own x86-64 code generator emitting machine code directly
  through its own instruction encoder, following the System V AMD64 ABI
  (C-501). No .s text in the build path, no as, no ld, no cc, no
  third-party backend. Human-readable assembly text survives only as a
  debug dump (--emit-asm), not as a build step.
- Binary format: internal ELF writer producing static executables — program
  headers and sections, no dynamic section, no relocation processing in
  v0.1.0 (whole-program compile in one invocation; the early-Go internal-
  linking posture, C-420's self-contained toolchain lesson).
- Runtime: kavirrt — a minimal freestanding runtime owned by the compiler:
  _start entry, direct Linux syscalls (exit/exit_group, write, read,
  brk/mmap for the allocator; the set grows only by explicit decision),
  C-516 freestanding posture. First allocator: bump allocator; free-list
  later. No libc headers, objects, or startup files ever enter the
  compiled-program path.
- IR: Kavir IR — small, typed, SSA-based, target-neutral; owned by the
  frontend; backends are consumers only. IR changes must be justified by
  frontend semantics; a backend convenience that reshapes the IR requires
  a decision-log entry first (anti-erosion rule; the C-414/C-415 pattern).
- Second-backend readiness: the IR plus IR-level snapshot conformance tests
  are the seam. A second backend (another target such as arm64, or another
  codegen tier) is additive by design. Whether any FUTURE tier may itself
  be a third-party backend (LLVM/Cranelift/QBE) is LEFT OPEN and reserved
  to the user — this decision bans third-party backends in the shipped
  v0.x path and does not pre-judge that later question.
- Compiler implementation language: C17 (unchanged: the development sandbox
  has gcc only; a C compiler with no runtime dependency matches the
  freestanding posture; Zig's bootstrap history shows C as a portable
  substrate, C-421). Boundary note, explicit: the no-cc constraint applies
  to the path of COMPILED KAVIR PROGRAMS. The compiler itself is built with
  gcc like any C program, and CI may build gcc-compiled reference binaries
  to cross-check generated-program behaviour — neither is part of the
  compiled-program path.

Costs (accepted with open eyes, per the user's request to record them):

1. Performance is unknown and starts naive. We lose QBE's/LLVM's optimizers
   wholesale; E1 measured QBE, not our codegen — there is NO evidence today
   that our encoder + allocator reaches even QBE's 70%-class output
   (C-413). The falsification gates below keep this honest.
2. Engineering effort concentrates in the compiler: the encoder, register
   allocator, ELF writer and runtime replace what was a thin emitter. This
   is the single largest Phase-3 work item and carries real schedule risk
   for the v0.1.0 slice (ADR-005's codegen line is amended accordingly).
3. Target breadth loss: with QBE, arm64/riscv64 were nearly free; now a
   second target means porting the code generator. Tier-1 is
   x86_64-linux only for v0.1.0.
4. ABI correctness risk sits on us: SysV argument classification, 16-byte
   stack alignment, aggregate passing (C-501; per-target ABI divergence is
   documented at C-502..C-506), and ABI breaks happen even in conservative
   ecosystems (C-518). Mitigation: conformance tests cross-checked against
   gcc-compiled reference binaries in CI (test-only use of cc).
5. No-libc consequences: no stdio/printf, no sanitizer runtimes for
   generated code, Linux-kernel-specific binaries; the debugging story is
   limited (hand-emitted line tables at best; no full DWARF in v0.1.0).
   Illegal behaviour in generated programs gets a trap mode (ud2 +
   exit_group) in debug builds instead. Without libc, C-library FFI is out
   of v0.1.0 scope; the FFI/ABI ADR must define the freestanding interop
   story (E5's QBE/cc FFI result becomes historical context).
6. History risk: the custom-backend path is exactly the one the evidence
   base warns requires institutional staying power (C-419/C-420 — Go had
   funding, a dedicated team, and still took years). This risk is accepted
   by user decision; the thin IR is the mitigation that keeps the door
   open.
7. "Thin IR" erosion risk: pressure from codegen convenience can deform the
   IR into a target-shaped one. Mitigation: IR snapshot tests plus the
   anti-erosion rule above.

Upside recorded honestly: with no external toolchain in the compiled path,
bit-reproducible output depends only on our compiler — the reproducibility
goal (C-708; C-709's toolchain-variance lesson; our E4 findings) becomes
materially easier to guarantee than in any cc-based pipeline.

Reversibility (cheap / costly / one-way): the execution model and the
binary format are one-way once public programs exist. The compiler
implementation language is costly to reverse, not one-way. Adding a second
backend is additive by design.

What result would show this was wrong (falsification gates):
1. Performance gate: if at v0.1.0 the benchmark suite shows our generated
   code >10x slower than gcc -O2 on the E1-style representative workloads,
   escalate optimization work or re-open the tier-2 question with the user.
2. Schedule gate: if the codegen slice (encoder + allocator + ELF writer +
   kavirrt) cannot pass the Phase-3 conformance smoke within the ADR-005
   time budget, return to the user with Option D (binutils as/ld) as an
   explicit re-decision — no silent fallback.
3. IR-purity gate: any backend-driven IR change without a prior
   decision-log entry fails review by definition.

Evidence IDs: C-201, C-204, C-205, C-207, C-210, C-402, C-403, C-405, C-409,
C-413, C-414, C-415, C-419, C-420, C-421, C-425, C-501, C-502, C-503, C-504,
C-505, C-506, C-516, C-517, C-518, C-708, C-709
Local artifacts: benchmarks/e1-backend-probe.md, benchmarks/e4-reproducibility.md,
benchmarks/e5-ffi-probe.md

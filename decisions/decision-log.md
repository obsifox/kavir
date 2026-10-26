# Decision log

One line per ADR: number, title, status, date. Superseded decisions stay listed.

## ADRs

- 2026-10-26: ADR-001 purpose/scope/non-goals — ACCEPTED (grounds: user gate
  approval; formalizes the approved recommendation).
- 2026-10-26: ADR-002 implementation strategy (v1: QBE-first native AOT,
  thin IR, compiler in C17) — PROPOSED, then REJECTED by the user.
- 2026-10-26: ADR-002 implementation strategy (v2, rewritten per user
  constraint: custom backend, direct x86-64 machine-code emission, internal
  ELF writer, thin IR, no libc, no cc/C path, Linux x86_64 only,
  second-backend-ready) — ACCEPTED (user decision, same day).
- 2026-10-26: ADR-003 memory & safety model (ownership + explicit unsafe, no
  GC, arenas as library) — ACCEPTED as direction (user decision); v0.1.0
  implemented subset + ordered cut list specified by the coordinator per
  the user's instruction (lexical borrows, no lifetime inference, no
  borrow returns, no field borrows; cuts: unsafe surface -> user borrow
  params -> &mut).
- 2026-10-26: ADR-004 core type-system direction (static-first, sound safe
  subset, Option[T], ADTs, monomorphizing generics, value types now) —
  ACCEPTED as direction (user decision); v0.1.0 implemented subset +
  ordered cut list specified by the coordinator per the user's instruction
  (cuts: advanced patterns -> user generic fns -> user generic types ->
  narrow-width ints).
- 2026-10-26: ADR-005 v0.1.0 scope v1 (narrow vertical slice, fixed bundle
  order) — REJECTED by the user: milestone order must be a walking
  skeleton with a cost estimate for the custom backend.
- 2026-10-26: ADR-005 v2 (walking skeleton M1..M7, LOC estimate ~9-15k,
  scope = M6 minus ADR-003/004 cuts) — ACCEPTED by the user WITH
  amendments; recorded as v3.
- 2026-10-26: ADR-005 v3 (v2 + user amendments: (a) calibration checkpoint
  moved to M3 — actual LOC/sessions/time vs estimate, scope re-derived
  there; (b) IR interpreter as correctness reference from M1 — every
  codegen test must match interpreter behaviour, frontend fuzz under
  ASan+UBSan, cost added to estimate (total ~10-16k LOC); (c) M5-cut
  contingency defined IN ADVANCE with objective trigger and the list of
  README claims forbidden in 0.1.0-m5cut (no memory-safety claims); (d)
  README single-platform statement — done) — ACCEPTED; M1 authorized to
  start immediately without re-approval; stop only for NEW irreversible
  decisions.
- 2026-10-26: ADR-006 platform roadmap (T2 Linux ARM64 -> T3 Wasm/WASI ->
  T4 Windows x64 -> T5 macOS ARM64; cost/risk per target; additive-
  architecture audit + per-target-module discipline rule; Wasm placed
  second with rationale; v0.1.0 scope unchanged) — ACCEPTED; user
  CONFIRMED the accepted order (ARM64, Wasm, Windows, macOS) the same
  day: planning only, v0.1.0 unchanged.

## User decisions (recorded in chat, not ADRs)

- 2026-10-26: feasibility gate APPROVED by the user (chat reply "قبوله") —
  Phase 1 → Phase 2.
- 2026-10-26: working name changed from "Blup" to "Kavir" by the user (name/license are
  user-owned checkpoints); due diligence in name-due-diligence.md.
- 2026-10-26: user REJECTED the QBE-first backend recommendation and imposed
  binding constraints (custom backend, direct machine code, no C path/cc,
  no LLVM/Cranelift/QBE, Linux x86-64 first, thin IR, no libc/direct
  syscalls, second backend addable) — ADR-002 rewritten and accepted on
  these constraints.
- 2026-10-26: ADR-003/004/005 one-line summaries (recommendation, evidence
  IDs, main rejected option) presented; user will answer each SEPARATELY —
  checkpoint 2 remains open for these three.
- 2026-10-26: user ANSWERED separately — (1) compiler in C17 + gcc ACCEPTED
  with the binding condition that compiled-program output NEVER passes cc
  (matches the ADR-002 v2 boundary note); (2) ADR-003 and ADR-004 ACCEPTED
  as directions, with the v0.1.0 subset + cut list delegated to the
  coordinator (now specified in the ADR texts); (3) ADR-005 v1 REJECTED —
  walking-skeleton order + codegen/ELF/runtime cost estimate required;
  v2 re-proposed on that basis, awaiting approval.
- 2026-10-26: M1.1 stabilization (independent review of M1). Findings:
  (1) lexer signed-overflow UB (2^63 wrapped negative past the checker);
  (2) boundary literals not permanently tested; (3) fuzz too shallow;
  (4) interpreter+ELF share one checker output (shared-blind-spot);
  (5) run.sh pass counter wrong; over-long identifiers silently
  truncated; `build -o out in.k` misparsed; (6) binaries tracked in git,
  hardcoded /home/z paths in benchmarks, LOC definition loose.
  ALL FIXED in M1.1; see reports/milestones/M1.md section "M1.1".
- 2026-10-26: ADR-007 (integer overflow semantics + M2 independent test
  oracle) — ACCEPTED, decisions DELEGATED by the user in the M1.1
  instruction. Overflow = compile-time error for non-i32 literals;
  runtime arithmetic overflow = defined TRAP (not silent wrap); lexer
  made UB-free (unsigned, check-before-multiply). M2+ conformance adds
  an independent oracle: three-way comparison (ELF == interpreter ==
  separately computed expectation).
- 2026-10-26: REPORTED to user, awaiting decision (no change made):
  README.md and STATE.md describe the repository as private, but the
  GitHub repo obsifox/kavir is currently PUBLIC. Per user instruction
  ("report and wait; do not change the status"), neither the repo
  visibility nor the two documents have been touched.

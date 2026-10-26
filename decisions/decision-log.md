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
- 2026-10-26: USER DECISIONS recorded (M2 kickoff message): ADR-007
  ACCEPTED by the user; git history must NOT be rewritten; repo
  visibility unchanged and README.md/STATE.md FROZEN until the user
  announces the private/public decision; language name final (Kavir);
  licensing fully set aside (no LICENSE/SPDX/clauses, none to be
  suggested); GitHub token env-var-only, never written anywhere;
  M1.1/M1.2 reclassified as AUTHOR-APPROVED (independent verification
  of 28ba9f1 will be done by a third party).
- 2026-10-26: ADR-008 (M2 output/arithmetic/if-else runtime contract)
  ACCEPTED under the user's delegation for small decisions; scope
  itself user-mandated. Pins: trap exit code 101 + exact trap message
  bytes (fills the ADR-007 placeholder); NO constant folding of
  arithmetic in M2 (keeps the ADR-007 trap path real and testable);
  division truncates toward zero; INT32_MIN / -1 and INT32_MIN % -1
  trap; print adds no newline; unary minus is literal-only with the
  2147483648 special case defining INT32_MIN; surface grammar recorded
  as TEMPORARY (syntax is finalized LAST per the user's rule); IR
  extended additively (M1 opcodes unchanged; IR is internal and never
  persisted); no RW PT_LOAD in M2 (nothing needs it yet).
- 2026-10-26: ADR-009 (source file extension) PROPOSED with collision
  research: .kv collides with Kivy (required extension), .k with
  kdb+/q's K language; candidates .kavir (no collision), .kvr
  (effectively unclaimed), .kav (no hard collision, soft Kaspersky-
  abbreviation concern). DECISION RESERVED TO THE USER; the repo keeps
  the temporary .k naming until then.
- 2026-10-26: USER DECISIONS (second message): (1) the M2 IR growth
  (8 opcodes) ACCEPTED — re-evaluate at M3; (2) ADR-009 ACCEPTED with
  the extension staying `.k` — nothing renamed (informed choice, the
  kdb+/q collision was presented); (3) repo visibility unchanged;
  README/STATE partially unfrozen ONLY for M2 status + the
  temporary-syntax note, private/public lines untouched; (4) no
  license of any kind, no history rewrite; M2 classified
  AUTHOR-APPROVED (third-party verification on pinned hashes).
- 2026-10-26: M3 scope NARROWED by the user (supersedes the ADR-005 v3
  M3 line for M3 only): comparison operators, while, let (local
  variable), user function calls + recursion (fib) — nothing else
  (no loop/break/continue). Calling convention + frame layout treated
  as a near-one-way door BY THE USER: ADR-010 drafted and STOPPED for
  the user's decision before implementation.

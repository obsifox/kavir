phase: 2 (design ADRs; pre-ADR experiments first)
gate: passed (user approved 2026-10-26, chat reply "قبوله")
last_session: 2026-10-26
covered_workstreams: [W1, W2, W3, W4, W5, W6, W7, W8]

Summary of last session:
- USER GATE APPROVED (2026-10-26, "قبوله"). Phase 2 opened.
- Pre-ADR experiments DONE (benchmarks/): E1 backend probe GCC vs QBE
  (checksums identical; QBE = 57% of the -O0->-O2 runtime win at lower
  compile cost; two-tier runtime gap 4.79x confirmed; LLVM/Cranelift legs
  impossible in sandbox — no root/rustc, recorded as unknown), E4
  reproducibility (confirmed C-708/C-709; bonus finding: '-ffile-prefix-map=.'
  naive form silently fails — absolute form required), E5 FFI smoke (QBE
  calloc/printf-varargs/qsort-with-QBE-callback all worked).
- ADR-001 (purpose/scope/non-goals) ACCEPTED (grounds: gate approval).
- ADR-002 implementation strategy, ADR-003 memory model, ADR-004 type system,
  ADR-005 v0.1.0 scope: DRAFTED as PROPOSED — presented to user (skill
  checkpoint 2: one-way-door decisions, each with recommendation).
- ADR-002 RESOLVED: user REJECTED the QBE-first recommendation and imposed
  binding constraints (custom backend, direct x86-64 machine code, no C
  path/cc, no LLVM/Cranelift/QBE, Linux x86-64 only, thin IR, no libc —
  direct syscalls, second backend addable). ADR-002 v2 REWRITTEN and
  ACCEPTED same day; costs/risks recorded honestly (perf unknown, largest
  Phase-3 work item, ABI risk on us, arm64 deferred, no-libc consequences,
  Go history caution); falsification gates: perf >10x vs gcc -O2, schedule
  gate with explicit as/ld fallback re-decision, IR-purity gate. ADR-005
  QBE references amended for consistency.
- ADR-003/004/005: one-line summaries (recommendation + evidence IDs + main
  rejected option) presented; user will answer EACH SEPARATELY.
- check_evidence.py normal + --gate: 0 errors (3 known warnings).
- Compiler implementation language: C17 (sandbox has gcc only). Boundary per
  ADR-002 v2: no-cc applies to compiled Kavir programs; the compiler itself
  is bootstrapped with gcc.

Open questions:
- Trademark database search (USPTO/EUIPO/WIPO) — user task before public release.
- Experiments: E1 backend probe limited in this sandbox (no root → no clang;
  no rustc → no Cranelift driver); GCC vs QBE only. Ownership-ergonomics probe
  (E2) blocked without Rust toolchain — recorded, deferred.
- Experiments before architecture decisions: backend probe (LLVM/Cranelift/QBE),
  ownership-ergonomics probe, lockfile reproducibility probe, FFI probe
  (feasibility-report section 8). NOTE: E1/E5 ran QBE/cc paths; under
  ADR-002 v2 their role is historical context/baselines, not the build path.
- Region-allocation literature (S-630) and HOPL papers (S-736) still unfetched leads.

Open sub-question (reserved to user): whether any FUTURE tier-2 backend may
be third-party (LLVM/Cranelift/QBE); ADR-002 v2 bans third-party backends
in the shipped v0.x path only.

Next three steps:
1. USER CHECKPOINT 2 continues: user answers ADR-003/004/005 separately
   (summaries presented 2026-10-26); flip statuses to accepted and log in
   decision-log.md.
2. Remaining ADRs (not one-way doors, Claude's call with evidence): error
   handling (Result+match, no exceptions), concurrency (postpone past
   v0.1.0), modules/build/versioning (lockfile+checksums), FFI/ABI (MUST
   now define a no-libc freestanding interop story per ADR-002 v2 —
   C-library FFI is out of v0.1.0), surface syntax (last, cheapest).
3. Phase 3 prototype slice per ADR-002 v2: kavir compiler in C17 → Kavir
   IR → own x86-64 encoder → internal ELF writer + kavirrt (_start +
   direct syscalls); lexer→parser→checker→codegen with conformance suite +
   fuzz targets from day one (per ADR-005 scope).

GitHub: private repo obsifox/kavir (remote origin). Token handled via env var only,
never stored in tracked files; not revoked or deleted (user instruction).

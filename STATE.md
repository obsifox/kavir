phase: 3 (M1 + M1.1 + M2 + M3 + M3.1 + M4 + M4.1 + M4.2 COMPLETE --
       author-verified; verification round 1 (2026-10-27) confirmed
       HEAD ef766ec, M4.1 scope ONLY; round 2 (2026-10-27) found 3
       real bugs on ef766ec, all fixed in M4.2 (sret/stack-param
       SIGSEGV, nested-match arm-pool corruption, arm cap counted
       program-wide) plus 2 more found by the new suites (label
       one-past-end, Option.Some diagnostic UB); M5 BLOCKED until
       verification round 3 finishes per user instruction
       2026-10-27; pushed to origin: through ef766ec + backup tag
       backup-m4-20261027)
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
- ADR-003/004: one-line summaries (recommendation + evidence IDs + main
  rejected option) presented; user answered SEPARATELY: BOTH ACCEPTED as
  directions, with v0.1.0 subset + ordered cut lists specified by the
  coordinator per user instruction (ADR-003: lexical borrows, no lifetime
  inference, no borrow returns, no field borrows; cut order unsafe ->
  user borrow params -> &mut. ADR-004: minimal monomorphizing core, no
  traits/bounds; cut order advanced patterns -> user generic fns -> user
  generic types -> narrow ints).
- ADR-005 v1 REJECTED by user: milestone order must be a WALKING SKELETON
  (M1 = `return 42` lexer→ELF) and the codegen/ELF/runtime cost must be
  estimated first. ADR-005 v2 re-proposed on that basis: LOC estimate
  ~9-15k (codegen 3-5k, ELF 0.5-1k, kavirrt 0.3-0.6k, frontend 4-6k,
  tooling 1-2k), ladder M1..M7, scope = M6 minus ADR-003/004 cuts.
  AWAITING USER APPROVAL; on approval M1 starts Phase 3.
- ADR-006 platform roadmap ACCEPTED (user-directed): long-term goal = all
  platforms; order T2 Linux ARM64 -> T3 Wasm/WASI -> T4 Windows x64 ->
  T5 macOS ARM64 with per-target cost/risk; additivity audit lists every
  x86-64/Linux coupling (syscalls, ELF, encoder, SysV ABI, 8-byte layout,
  CI runner, brk) with isolation plans (sys_<target>/abi_<target>/
  emit_<format>/backend_<arch> modules + Target descriptor); discipline
  rule: target constants only inside per-target modules from v0.1.x, no
  speculative abstractions before T2; Wasm second (cheapest verification,
  preserves freestanding posture, real test of ADR-002's second-backend
  promise; C-426 hypothesis cited, re-evaluate at T3). README gained a
  Supported platforms section (WSL2/VM/container guidance for Windows/
  macOS users). v0.1.0 scope UNCHANGED.
- User also confirmed: compiler bootstrap in C17+gcc accepted with the
  binding condition that compiled-program output NEVER passes cc.
- ADR-005 v3 ACCEPTED with user amendments: (a) calibration checkpoint at
  M3; (b) IR interpreter as correctness reference from M1 + frontend fuzz
  under ASan+UBSan (estimate raised to ~10-16k LOC); (c) m5cut contingency
  defined in advance (objective trigger + forbidden README claims); (d)
  README single-platform statement done. M1 authorized to start
  immediately without re-approval (stop only for NEW irreversible
  decisions).
- M1 COMPLETE (same session): compiler/ — C17 pipeline lexer→parser→
  checker→IR→own x86-64 encoder→internal static ELF; `return 42` runs
  end-to-end: 145-byte ELF exits 42; readelf-verified; interpreter
  cross-check green (elf=42 interp=42); 3 negative tests with span
  diagnostics; IR dump; 300-iteration sanitizer fuzz: 0 problems.
  1271 LOC total (pre-M1.1 measurement; see the M1.1 bullet for the
  post-fix count with its definition). No new irreversible decisions
  (see reports/milestones/M1.md). First push of real compiler code.
- M1.1 stabilization COMPLETE (2026-10-26, response to the independent
  M1 review): lexer overflow bug fixed (unsigned check-before-multiply;
  Token/Expr/checker unsigned; 2^63 now errors like 2147483648); 9
  permanent boundary fixtures + pattern-checked diagnostics (16/16
  checks); fuzz upgraded (grammar-aware generator w/ long digit strings
  + mutations; 8000 inputs / 24000 execs / 90.5 s / 0 problems) and a
  clang-only libFuzzer harness added in-repo (no clang here — honest
  limits recorded); run.sh counter fixed; over-long identifiers give a
  distinct error; `build -o out in.k` works; binaries untracked +
  .gitignore; benchmarks env-configurable; LOC definition explicit
  (1165 C LOC = 1032 .c + 133 hdr; sh scripts/loc.sh). ADR-007 ACCEPTED
  (user-delegated): literal overflow = compile error; runtime overflow
  (M2+) = TRAP; M2+ adds an independent oracle (three-way: ELF ==
  interpreter == separately computed expectation).
- check_evidence.py normal + --gate: 0 errors (3 known warnings).
- Compiler implementation language: C17 (sandbox has gcc only). Boundary per
  ADR-002 v2: no-cc applies to compiled Kavir programs; the compiler itself
  is bootstrapped with gcc.
- M2 COMPLETE (commit 8aa46cd, LOCAL ONLY — push pending GH_TOKEN):
  print via write syscall (string + i32 decimal), + - * / % with
  runtime trap (exit 101, byte-pinned messages, ADR-007/008), if/else;
  IR extended by 8 opcodes (additive); three-way oracle (ELF ==
  interpreter == tests/reference.py INDEPENDENT expectation), byte-exact
  stdout/stderr/exit; 40 M2 fixtures + 16 M1 checks green; fuzz with
  real statistics only (80k differential inputs, 20k ASan inputs, 2x301 s
  libFuzzer ASan+UBSan — no findings). M2 is AUTHOR-APPROVED ONLY; no
  independent-verification claim is made. Details:
  reports/milestones/M2.md.
- USER DECISIONS (2026-10-26, second message): (1) the 8-opcode M2 IR
  growth ACCEPTED; re-evaluate at M3. (2) Source extension stays `.k`
  (ADR-009 ACCEPTED; nothing renamed). (3) Repo visibility NOT changed;
  README/STATE refreshed for M2 status + temporary-syntax note WITHOUT
  touching the private/public lines (this file's OPEN block below stays
  verbatim). (4) No license of any kind; no history rewrite. M2 status
  = author-approved; independent verification is a third-party task on
  pinned hashes.
- M3 STARTED. Scope narrowed BY THE USER to: comparison operators,
  while, let (local variable), user function calls + recursion (fib)
  — nothing else (loop/break/continue excluded). Calling convention +
  frame layout declared a near-one-way door BY THE USER: ADR-010 draft
  written (options + evidence + recommendation) and implementation
  STOPPED for the user's choice before anything is locked.
- ADR-010 ACCEPTED via the user's structured gate answers: SysV AMD64
  integer subset (RDI/RSI/RDX/RCX/R8/R9, stack args right-to-left,
  EAX return), RBP frames, recursion depth limit 1024 (user chose the
  higher limit over the recommended 256), and let/let mut per the
  ADR-005 v3 wording (user chose the pair over the recommended single
  mutable let).
- M3 COMPLETE (same session): comparisons (1/0, non-associative),
  while, let/let mut with immutable params and flat scopes, calls
  0..8 args with forward refs + mutual recursion, early return
  anywhere (M2 rule relaxed, fixture moved), fall-off = 0; IR +8
  opcodes (LT/LE/GT/GE/EQ/NE/COPY/CALL); interpreter rewritten to an
  explicit frame stack (frame index == call depth, mirrors the hidden
  R11D depth argument checked in every ELF prologue). Six bugs found
  and fixed during bring-up (RET stopped codegen; global IR labels;
  name-index-vs-vreg mapping; per-function adhoc label collision;
  ELF depth off-by-one; two oracle-only bugs) — all caught by the
  three-way contract or fuzz before any claim was made.
- M3 verification (real numbers): run_m3 41/41, run_m2 39/39 (one
  fixture moved to M3), run.sh 16/16; differential fuzz 80,000 inputs
  (seed 20261026) → 46,879 accepted three-way, 33,121 rejected,
  0 problems, 178.7 s; ASan+UBSan fuzz 20,000 inputs (seed 424242) ×
  3 execs = 60,000 execs, 0 findings, 274.7 s. libFuzzer NOT RUN in
  this session (no clang; two zig-workaround attempts failed at link
  time) — recorded honestly; the verifier must run
  `make kavir-libfuzzer && make fuzz-libfuzzer`.
- M3 CALIBRATION CHECKPOINT (ADR-005 v3 amendments a+c): C LOC 3283
  (3034 .c + 249 hdr; definition in scripts/loc.sh; was 2358 after
  M2, delta +925); tooling PY 1835; suite SH 101; fixtures 13+39+41.
  vs the estimate table: backend+ELF+runtime 863 (est. 3.8–6.6k),
  frontend 1722 (est. 4–6k), interpreter 249 (est. 1–1.5k), tooling
  ≈2014 (est. 2–3k) — no falsification signal; ADR-002 schedule gate
  (as/ld fallback) evaluated and NOT needed. m5cut trigger evaluated:
  DOES NOT FIRE (remaining ≈6–12k vs 2×5.3k = 10.6k; midpoint below
  the trigger, upper edge would fire — margin not huge; M4/M5 are the
  honest tests). Coordinator opinion recorded in M3.md: KEEP v0.1.0
  scope; next scope conversation at M4 completion.

- M3.1 HARDENING COMPLETE (2026-10-26, user-specified scope, no new
  language features): (1) backend label allocation redesigned to ONE
  sequential allocator with per-class intern tables — the carved
  hand-maintained ranges (two counters + magic constants) that caused
  both M3 label bugs are structurally gone; targeted 50-function
  fixtures (branch + division per fn, 3 whiles, trap sites in several
  fns, one div-zero and one overflow variant) added; the redesign's
  own first version broke while back-edges and was caught by run_m3
  before any claim. (2) Permanent depth-boundary tests: 1024 frames
  legal / entering frame 1024 traps (names the callee, exit 101),
  depth-1025 clamp, stdout-order-before-trap, mutual-recursion both
  sides of the boundary. (3) Arg-passing: ADR-010 §6 amendment (no
  6-vs-8 contradiction existed; split now explicit: 6 register + up
  to 2 stack, ((nstack+1)&~1)*8 rounding); weighted 6/7/8-arg +
  bit-encoded stack-order fixtures; new tests/run_m31.py verifies
  every sub/add rsp immediate is a 16-multiple in the emitted ELF
  (5/5). (4) libFuzzer NOT RUN for M3/M3.1 (no clang, no sudo) — no
  "fuzz clean" phrasing anywhere. (5) LOC-extrapolation caveat added
  to the M3 calibration. Capacities raised (ADR-008 §7 amendment):
  TOKENS 2048, FNS 64, BLOCKS 256, EXPRS 1024. Re-verification on the
  new build: M1 16/16, M2 39/39, M3 51/51, run_m31 5/5; diff fuzz
  80,000 inputs (seed 20261026) → 59,037 accepted three-way, 20,963
  rejected, 0 problems, 256.9 s; ASan fuzz 20,000 × 3 = 60,000 execs
  (seed 424242), 0 findings, 289.1 s. C LOC 3369 (+86). M3/M3.1 stay
  AUTHOR-APPROVED ONLY.

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
1. M4 Aggregates per ADR-005 v3: structs by value (SysV integer-
   register classes), enums as tagged sums, match with exhaustiveness,
   Option[T] — each introduced through the same three-way contract;
   any NEW one-way door (e.g. aggregate ABI classification details)
   stops for the user first.
2. At M4 completion: re-check the m5cut trigger with aggregate costs
   measured (the first scope conversation that matters).
3. Push pending GH_TOKEN (8aa46cd, 4d0921b, M3 commit, M3.1 commit —
   token env-var-only, never written anywhere); third-party
   verification continues on pinned hashes (M3.md has the exact
   commands).

OPEN (user decision pending, NOTHING changed 2026-10-26): README.md and
STATE.md call the repo private but obsifox/kavir is currently PUBLIC.
Reported to the user per instruction ("report and wait; do not change
the status"). The line below is intentionally left as-is until the user
decides.

## M4 STATUS (2026-10-27)

M4 COMPLETE; pushed to origin/main as 365bc13 (2026-10-27; token
env-var-only, never written to any file). Scope per the user's four
locked decisions (ADR-011, 2026-10-27): structs by value (C layout),
enums with i32 tags, exhaustive match (flat patterns, i32 arms),
Option[T] monomorphisation; values > 8 bytes ALWAYS through memory
(a documented deviation from the psABI two-register class for
9..16 bytes); values <= 8 bytes in ONE register like a SysV INTEGER
eightbyte.

Verification (author-approved only):
- three-way: M4 36/36, M3 51/51, M2 39/39, M1 16/16; ELF structural
  M4 6/6, M3.1 5/5 (every sub/add rsp imm = 16-multiple).
- fuzz diff: 80,000 inputs (seed 20261027) -> 57,063 accepted checked
  three-way, 22,937 clean rejects, 0 problems, 301.6 s.
- fuzz ASan+UBSan: 20,000 x3 = 60,000 execs (seed 777007), 0
  findings, 314.0 s.
- libFuzzer NOT RUN (no clang; no passwordless sudo) — recorded
  honestly; the verifier command is in reports/milestones/M4.md.
- LOC: C 5430 (5035 .c + 395 kavir.h, +2061 vs M3.1); tooling PY
  3257; fixtures 13+39+51+36.
- m5cut re-check at M4 completion (user requirement): remaining
  projected 3.2-9.4k C LOC vs threshold 10,860 (= 2 x 5430) — NOT
  FIRED (margin 1.46k at the upper edge); pre-agreed shrink plan
  recorded in M4.md (M5 borrow diagnostics reduce to move-only; M6
  builtins-only monomorphisation; formatter deferred).

Next: M5 Memory (arena, slices, move/XOR per the ADR-003 subset).

GitHub: private repo obsifox/kavir (remote origin). Token handled via env var only,
never stored in tracked files; not revoked or deleted (user instruction).

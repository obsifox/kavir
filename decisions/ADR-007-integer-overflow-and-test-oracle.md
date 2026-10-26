# ADR-007: Integer overflow semantics and the M2 independent test oracle

- **Status:** Accepted (2026-10-26)
- **Decided by:** coordinator, explicitly delegated by the user in the
  M1.1 instruction ("determine what integer overflow means — wrap, trap,
  or compile-time error — and record it in an ADR"; "add an independent
  source of truth for M2 and record it in an ADR/report").
- **Triggered by:** the M1.1 independent review finding that the IR
  interpreter and the ELF backend both consume the SAME checked program,
  so frontend bugs (e.g. the M1 lexer signed-overflow bug, where
  `9223372036854775808` wrapped negative and slipped past the checker)
  are invisible to the cross-check; and that arithmetic (M2) must not
  start without defined overflow semantics.
- **Scope:** v0.1.0 (i32 only). Wider integer types would re-open this ADR.

## Context

M2 introduces integer arithmetic (`+ - * / %`) on i32. Two questions had
to be settled BEFORE any M2 code exists, because both are load-bearing
for the checker, the IR, the backend lowering, the interpreter, and the
conformance suite:

1. What happens when a source-level integer literal is too large?
2. What happens when a runtime arithmetic operation overflows?

And separately: the M1 verification design compares the compiled ELF
against the IR interpreter, but both consume the same checker output — a
single frontend bug can change BOTH sides of the comparison consistently
and stay invisible. That is a structural weakness of the two-way
comparison, demonstrated concretely by the M1 lexer bug (it produced a
wrong value that both the interpreter and the ELF would have agreed on).

## Decision 1 — Integer overflow semantics

Three-layer rule:

| Layer | Situation | Behaviour |
|-------|-----------|-----------|
| Lexing | Digit string exceeds u64 (e.g. 2^64, 10^20) | Compile-time ERROR "integer literal too large" (distinct message) |
| Checking | Literal fits u64 but not i32 (e.g. 2^31, 2^63, 2^63+1, 2^64-1) | Compile-time ERROR "integer literal N does not fit in i32 (range -2147483648 .. 2147483647)" |
| Runtime (M2+) | Arithmetic result of `+ - * / %` on i32 does not fit in i32 | **TRAP**: defined, loud failure |

Runtime trap definition (binding for M2):
- The compiled program aborts through a dedicated trap path: writes a
  one-line diagnostic `trap: integer overflow at <file>:<line>:<col>` to
  fd 2, then `exit_group` with a FIXED non-zero exit code (to be pinned
  when the runtime lands in M2; the interpreter must use the same code).
- The IR interpreter raises the identical trap for the identical inputs.
  Conformance tests compare ELF vs interpreter vs independent expectation
  on trap cases too.
- Implementation note (x86-64): overflow checks are one conditional jump
  per arithmetic operation into a cold trap path (`jo` after `add/sub/imul`
  — flags are set anyway); `INT_MIN / -1` and `INT_MIN % -1` are checked
  explicitly since they trap without flags.

Rationale:
- **No silent wrap.** ADR-003's foundation is "no illegal silent
  behaviour"; a silently wrapping i32 is exactly the class of bug Kavir
  exists to reject. Wrap-on-overflow is the default in C and a known
  source of real vulnerabilities.
- **Not compile-time-only.** Most overflow involves runtime values;
  rejecting them statically is impossible without value ranges we do not
  have in v0.1.0. Constant-folded overflow IS rejected at compile time —
  today that is precisely the literal rule above.
- **Cheap on our backend.** The direct x86-64 encoder owns instruction
  selection (ADR-002 v2); a trap check costs one mostly-not-taken jump.
  This is the same trade-off Rust made for debug builds; if evidence
  later shows the cost matters, explicit wrapping intrinsics
  (`wrapping_add` etc.) are ADDITIVE and would not re-open this ADR.
- **Totality.** Every i32 operation now has exactly one defined outcome:
  a value in range, or a loud trap. There is no UB anywhere in Kavir's
  integer semantics.

Rejected alternatives:
- *Silent two's-complement wrap* — violates ADR-003 ethos; hides bugs.
- *Saturating arithmetic* — surprising, costlier, no supporting evidence
  in the corpus; also silently hides bugs (a quieter wrong answer).
- *Compile-time rejection of all possible overflow* — impossible for
  runtime values without a value-range analysis (not in v0.1.0 scope).

Also fixed as part of this ADR (M1.1): the lexer accumulates numerals in
`unsigned long long` and checks BEFORE each multiply, so no signed-overflow
UB is reachable anywhere in the frontend, and the checker's range test is
unsigned — values at/above 2^63 can never wrap into the accepted range.

## Decision 2 — M2 independent oracle (closing the shared-checker blind spot)

From M2 on, every conformance case carries an expected result that is
computed INDEPENDENTLY of the Kavir compiler:

1. **Independent expectation.** A separately written reference evaluator
   (`compiler/tests/reference.py`, Python) computes expected stdout /
   exit code / trap for each fixture and generated case. Its values are
   never derived from running the compiler or the interpreter.
2. **Three-way comparison.** For each case:
   `ELF behaviour == IR interpreter behaviour == independent expectation`.
   Any two-side agreement without the third is a failure, not a pass.
3. **Generator independence.** For generated arithmetic programs, the
   generator computes the expected value with Python's own arithmetic at
   generation time (deciding wrap/trap per this ADR), and the value is
   embedded in the generated fixture as a comment/metadata the runner
   checks against — so the program that produced the expectation is not
   the program under test.

Division of labour (none of the three replaces the others):
- *Interpreter* (amendment b, ADR-005 v3): executable reference semantics;
  catches backend lowering bugs.
- *Independent oracle* (this ADR): catches frontend bugs that would
  change both sides consistently — the M1.1 finding.
- *Fuzzing under sanitizers* (amendment b + M1.1): catches crashes, UB,
  and memory errors across all stages, including paths no fixture visits.

M1 status: already conforming in spirit — `tests/run.sh` expected values
(42, 255) are hand-written constants independent of the compiler output.
The Python reference evaluator itself lands with M2, where there are
actual arithmetic values to compute.

## Consequences

- The runtime grows a trap path in M2 (first real `kavirrt` work: the
  diagnostic string, the write(2), the fixed trap exit code). This was
  anticipated in the ADR-005 v3 estimate (~0.3–0.6k LOC runtime).
- The interpreter must implement trap semantics identically; the
  conformance suite gains trap cases as first-class tests.
- `tests/reference.py` is a THIRD implementation and can itself be wrong;
  disagreements are triaged explicitly and never resolved silently in the
  compiler's favour. This risk is accepted and is smaller than the blind
  spot it closes.
- Integer semantics are now total and UB-free by construction; any future
  integer type (i64, u32, ...) must come back to this ADR before M4.

## Evidence references

- **C-101** (verified): C defines integer overflow as undefined behaviour
  and UB "imposes no requirements" on the program — the concrete hazard
  that motivates banning silent wrap AND banning UB anywhere in Kavir's
  own integer semantics (the M1 lexer bug was exactly this UB class).
- **C-117** (verified): Zig replaces open-ended UB with a catalogue of
  Illegal Behaviour, integer overflow included, checked in safe builds —
  the closest precedent for the trap decision (loud, defined failure
  instead of wrap or UB).
- **C-108** (verified): Rust requires that no program, even inside
  unsafe, exhibits UB — supports the "UB-free by construction" posture
  this ADR commits Kavir to.
- The independent-oracle decision (Decision 2) is grounded in the M1.1
  review finding itself (structural shared-checker blind spot,
  demonstrated by the lexer bug) rather than in external literature; the
  three-way comparison design follows standard differential-testing
  practice.

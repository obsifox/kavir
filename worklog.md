
---
Task ID: 18 (M4 COMPLETE)
Agent: main (coordinator)
Task: M4 Aggregates per ADR-005 v3 M4 line + the user's four locked
  decisions (2026-10-27): struct (value), enum (tagged sums), match
  (exhaustive), Option[T] — C layout, i32 tag first, <=8B in one
  register, >8B ALWAYS memory.

Work Log:
- ADR-011 drafted and locked from the user's structured answers; the
  9..16B psABI deviation (memory instead of two registers) recorded
  as the user's deliberate choice.
- Full pipeline: lexer (+struct/enum/match/Option/./[]/=>/_ tokens),
  parser (type syntax, defs, struct literals, postfix chains on calls,
  Option ctors, match arms, field-path assignment), checker (type
  table, C layout, Option monomorphization by name, exhaustiveness,
  arm-local binds, copy-on-bind semantics), ir.c (aggregates lowered
  to CONSECUTIVE WORDS — no new IR opcodes; CALL carries per-arg and
  return word counts), interp (word-wise CALL/RET), backend (<=8B = ONE
  register packed low/high like a SysV INTEGER eightbyte; >8B = stack
  copy in 8B units + hidden-pointer return through a caller scratch
  area; R10 = the only pack scratch — RDX clobber bug found by the
  M4 fixtures and fixed).
- Oracle reference.py: full M4 surface (aggregate values with copy
  semantics, match, Option), independent.
- Suites: cases_m4 36 fixtures (12 generated positives + 24
  negatives), run_m4.py three-way, run_m4_elf.py structural (every
  sub/add rsp imm = 16-multiple; aggregate stack areas visible).
  Bugs found by the suites: postfix-on-call, nested struct-literal
  pool interleaving, Option pattern typing, arm-slot allocation order
  (nested match arms overwrote outer arms — arm slot claimed BEFORE
  body parse), RDX pack clobber, missing Some-payload type check,
  recursive-type detection needed the layout pass deferred until all
  forward refs resolve.
- Regression: M1 16/16, M2 39/39, M3 51/51, run_m31 5/5 — green.
- Fuzz (real numbers, M4 layer at 35% of diff-mode programs): diff
  80,000 inputs (seed 20261027) -> 57,063 accepted three-way checked,
  22,937 rejected, 0 problems, 301.6 s; ASan 20,000 x3 = 60,000 execs
  (seed 777007), 0 findings, 314.0 s. libFuzzer NOT RUN (no clang, no
  passwordless sudo — recorded honestly, verifier command in M4.md).
- LOC (sh scripts/loc.sh): C 5430 (5035 .c + 395 hdr; was 3369 after
  M3.1, +2061); tooling PY 3257; fixtures 13+39+51+36.

Stage Summary:
- M4 COMPLETE (local commit pending push). Still AUTHOR-APPROVED only.
- m5cut re-check: see M4.md (C LOC 5430 of 10.6k threshold 2x —
  computed below in the report).

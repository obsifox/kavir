# E4 — reproducibility probe results

Date: 2026-10-26. Pre-declared procedure and criteria in
`benchmarks/methodology.md`; harness: `e4-reproducibility/run_e4.py`
(same `probe.c` as E1). Toolchain: cc = gcc 14.2.0, Debian trixie.

## Results

| step | condition | sha256 relation |
|------|-----------|-----------------|
| (a)  | same dir, `cc -O2`, built twice | IDENTICAL (`c636d446…`) |
| (b)  | `cc -O2 -g`, two different build dirs | DIFFERENT (`efa21c50…` vs `4a5b8061…`) |
| (c)  | (b) + `-ffile-prefix-map=<abs-dir>=/usr/src/kavir-probe` | IDENTICAL (`478244d6…`) |

Verdict vs pre-declared criteria (a identical, b different, c identical):
**HYPOTHESIS CONFIRMED** — C-708/C-709 hold locally: deterministic output
requires explicit environment control; "same sources + lockfile" is not
sufficient once debug info is involved.

## Bonus finding (failed first attempt, kept deliberately)

The first run of step (c) used the naive form
`-ffile-prefix-map=.=/usr/src/kavir-probe` and the two builds remained
DIFFERENT: gcc records the *absolute* working directory as `DW_AT_comp_dir`,
so a `.`-relative map never matches. The corrected absolute-path form makes
the builds identical. Lesson recorded for ADR-008 (modules/build): a Kavir
build tool that promises reproducibility must normalize absolute paths itself
(or pass the correct map) — this is exactly the class of silent footgun the
evidence predicted, and it fails without any error message.

## Notes

- `probe.c` uses no `__DATE__`/`__TIME__`/`__FILE__`, so those hazards remain
  documented (C-708) rather than exercised here.
- Scope: compiler-level determinism only. Registry/dependency reproducibility
  (lockfiles, checksums) is covered by Phase 1 evidence (C-701..C-706) and by
  the tooling ADR, not by this probe.

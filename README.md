# Kavir -- new programming language (research workspace)

Research first, then design. See STATE.md for the current phase and gate.

- Charter: charter/charter.md
- Evidence ledger: evidence/sources.csv, evidence/claims.csv
- Decisions: decisions/ (decision-log.md; ADRs will appear after the feasibility gate)
- GitHub: private repository obsifox/kavir (public release is gated on v0.1.0 readiness)

## Status (2026-10-27)

- Milestones: M1 (walking skeleton), M1.1 (stabilization), M2 (output
  via write syscall, arithmetic with defined traps, if/else), M3
  (comparisons, while, let/let mut, function calls + recursion with a
  defined 1024-frame depth limit), M4 (structs, enums, exhaustive
  match, Option[T]) and the M4.1 hardening pass are COMPLETE --
  **verified by the author + the first independent-verification round
  (2026-10-27), which confirmed HEAD (M4.1 scope ONLY)**. No
  independent-verification claim is made for the M2-M4 milestone
  scopes until verification round 2 finishes (user instruction,
  2026-10-27); verification runs on pinned commits.
- Verification contract (ADR-005 v3 amendment b, ADR-007, ADR-008):
  every codegen test compares the compiled ELF against the IR
  interpreter AND an independently written Python oracle
  (compiler/tests/reference.py), byte-exact on stdout, stderr, and
  exit code.
- Fuzz-harness step budget (M4.1): the libFuzzer harness bounds each
  interpreted run; budget expiry is the explicit
  KAVIR_INTERP_STEP_LIMIT status of interp_run (no value, no output,
  NOT a trap). Harness-only by design: the CLI never sets a step
  limit and the three-way conformance contract never observes this
  status.
- Aggregate ABI (ADR-011): aggregate values (struct/enum/Option) up
  to 8 bytes pass in ONE integer register; aggregate values LARGER
  than 8 bytes ALWAYS pass through memory -- stack copy in 8-byte
  units for arguments, hidden-pointer return -- and are never split
  across two registers (deliberate deviation from the x86-64 psABI
  9..16-byte class; user decision, 2026-10-27).
- **The surface syntax is TEMPORARY.** Per the user's ordering rule,
  semantics are defined first and grammar is finalized last; do not
  write tutorials against the current grammar (ADR-008 section 0).
- Source file extension: `.k` -- FINAL per user decision (ADR-009).
  The toolchain itself does not enforce any extension.
- Licensing: intentionally none. No LICENSE file, no SPDX headers, no
  license clauses (user decision, 2026-10-26).

## Supported platforms

**v0.1.0 supports x86_64 Linux only.** Compiled programs are static ELF
executables with no libc dependency and no C toolchain anywhere in the
compiled-program path (ADR-002 v2).

| Platform | Status |
|----------|--------|
| x86_64 Linux | **Supported** (v0.1.0 target) |
| Linux ARM64 | Roadmap -- next target (ADR-006, T2) |
| WebAssembly (WASI) | Roadmap (ADR-006, T3) |
| Windows x64 | Roadmap (ADR-006, T4) |
| macOS ARM64 | Roadmap (ADR-006, T5) |

The full platform roadmap -- target order, cost and risk estimates, and the
additive-architecture audit -- is in
[decisions/ADR-006-platform-roadmap.md](decisions/ADR-006-platform-roadmap.md).

### If you are on Windows or macOS today

v0.1.0 binaries only run on x86_64 Linux, so until your platform ships you
can work with Kavir through any x86_64 Linux environment:

- **Windows:** WSL2 (recommended), or an x86_64 Linux virtual machine.
- **macOS (Apple Silicon):** an x86_64 Linux virtual machine (e.g. UTM,
  Lima/Colima, or Docker with `--platform linux/amd64` on another host).
- **Any OS:** an x86_64 Linux container (Docker/Podman) or a remote Linux
  box (VPS, CI runner).

Note: the `kavir` compiler itself is a C17 program and may be buildable on
other hosts, but v0.1.0 is only tested on Linux and can only emit
x86_64-linux binaries -- running the output requires an x86_64 Linux
runtime environment (one of the above).

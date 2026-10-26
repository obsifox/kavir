# Kavir — new programming language (research workspace)

Research first, then design. See STATE.md for the current phase and gate.

- Charter: charter/charter.md
- Evidence ledger: evidence/sources.csv, evidence/claims.csv
- Decisions: decisions/ (decision-log.md; ADRs will appear after the feasibility gate)
- GitHub: private repository obsifox/kavir (public release is gated on v0.1.0 readiness)

## Supported platforms

**v0.1.0 supports x86_64 Linux only.** Compiled programs are static ELF
executables with no libc dependency and no C toolchain anywhere in the
compiled-program path (ADR-002 v2).

| Platform | Status |
|----------|--------|
| x86_64 Linux | **Supported** (v0.1.0 target) |
| Linux ARM64 | Roadmap — next target (ADR-006, T2) |
| WebAssembly (WASI) | Roadmap (ADR-006, T3) |
| Windows x64 | Roadmap (ADR-006, T4) |
| macOS ARM64 | Roadmap (ADR-006, T5) |

The full platform roadmap — target order, cost and risk estimates, and the
additive-architecture audit — is in
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
x86_64-linux binaries — running the output requires an x86_64 Linux
runtime environment (one of the above).

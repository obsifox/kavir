# ADR-006: Platform roadmap — order, cost, risk, and additive-architecture audit

Status: accepted (user-directed roadmap 2026-10-26; target ordering
delegated to the coordinator; PLANNING ONLY — no change to the v0.1.0
scope of ADR-005 v2, and each target activation remains a separate future
decision)
Date: 2026-10-26
Question: How does Kavir get from "x86_64 Linux only" (v0.1.0) to the
user's long-term goal — write and compile Kavir on/for every platform —
and in what order, at what cost and risk?

User goal (binding direction, 2026-10-26): Kavir must become usable across
all platforms (writing code and compiling for each). v0.1.0 remains
x86_64-linux only per ADR-005 v2 — unchanged by this ADR.

Candidate targets: Linux ARM64, Windows x64, macOS ARM64, WebAssembly
(WASI).

## 1. Recommended order, cost, risk

  #  Target            New code (est.)   Risk    Main risks / reasons
  -- ----------------- ----------------- ------- -----------------------------
  2  Linux ARM64       ~1.5-2.5k LOC     LOW     New instruction encoder;
                                                 everything else reuses the
                                                 v0.1.0 posture (same kernel
                                                 ABI style, same ELF writer
                                                 with e_machine=EM_AARCH64,
                                                 64-bit layout unchanged).
                                                 AArch64 fixed-width 4-byte
                                                 encoding is easier to get
                                                 right than x86-64 variable-
                                                 length opcodes (C-503 ABI).
                                                 Verification: CI arm64
                                                 runners / qemu-user / SBCs.
  3  Wasm32 (WASI)     ~3-4k LOC         MEDIUM  Most different target: no
                                                 registers, linear memory,
                                                 structured control flow (IR
                                                 CFG -> wasm blocks needs a
                                                 restructuring pass), new
                                                 module encoder (not ELF).
                                                 But: smallest external
                                                 surface (WASI preview1:
                                                 proc_exit, fd_write,
                                                 fd_read, random_get,
                                                 clock_time_get, memory.grow
                                                 instead of brk), no libc,
                                                 no dynamic linking, no
                                                 codesigning; verification is
                                                 the CHEAPEST of all targets
                                                 (wasmtime runs everywhere).
                                                 Reach: browsers, embedded,
                                                 plugin ecosystems (C-411).
  4  Windows x64       ~1.5-2.5k LOC     MEDIUM  Encoder is REUSED (same
                                                 x86-64 ISA) but the no-libc
                                                 posture mutates: Windows has
                                                 no stable direct-syscall
                                                 ABI, so a minimal PE import
                                                 table (kernel32: ExitProcess,
                                                 WriteFile/GetStdHandle,
                                                 VirtualAlloc) replaces the
                                                 syscall layer; PE/COFF writer
                                                 with IAT replaces ELF.
                                                 Win64 ABI differences: args
                                                 in RCX/RDX/R8/R9, 32-byte
                                                 shadow store, by-reference
                                                 passing for >8-byte aggregates
                                                 (C-502). Loader/unwind
                                                 expectations are the opaque
                                                 risk area.
  5  macOS ARM64       ~2.5-4k LOC       HIGH    Highest external constraints:
                                                 no stable syscall ABI (must
                                                 call libSystem via dyld —
                                                 the no-libc posture breaks
                                                 here, honestly recorded),
                                                 Mach-O writer + LC_MAIN +
                                                 bind info, and Apple Silicon
                                                 requires at least ad-hoc
                                                 code signatures to run
                                                 binaries at all. Apple
                                                 hardware CI + notarization
                                                 friction. Deferred last on
                                                 purpose.

Ordering rationale: ARM64 first because it is the smallest honest delta on
the native axis (validates the encoder/ABI seams with the least new
machinery); Wasm second because it is the cheapest to VERIFY, keeps the
freestanding philosophy intact, and stress-tests IR neutrality
(ADR-002's second-backend promise) while the codebase is still small;
Windows third, once the platform seams (below) are proven by two prior
targets; macOS last, because its blockers are externally imposed and
cannot be engineered away by us.

## 2. Additivity audit — where the current design is tied to x86-64/Linux

The ADR-002 v2 seam (thin target-neutral IR -> backend consumers) is the
architecture; this audit lists every current coupling and its isolation
plan. Discipline rule during v0.1.x (no scope add): all target-specific
constants live ONLY in a single per-target module (targets/x86_64_linux.c
by v0.1.0's end); review treats any target constant outside such a module
as a defect. No speculative abstraction layers are built before target 2 —
the refactor happens at T2 kickoff.

  Component            Current x86-64/Linux coupling      Isolation plan
  -------------------- ---------------------------------- --------------------------
  kavirrt runtime       _start entry; syscall numbers       sys_<target> module with
  (runtime)             (exit/exit_group 60/231, write 1,   a fixed internal API
                        read 0, brk 12, mmap 9); syscall    (kavir_sys_write/exit/
                        instruction + RDI/RSI/RDX args      grow); per target:
                        (C-516 posture)                     Linux-arm64 svc/x8, WASI
                                                            imports, kernel32 via IAT,
                                                            libSystem on macOS
  ELF writer            ET_EXEC, e_machine=62, PT_LOAD      emit_<format> module
                        flags, 8-byte entries               (elf/pe/macho/wasm
                                                            encoder); whole-program,
                                                            no relocations — keep
                                                            this posture per target
                                                            where the loader allows
  Instruction encoder   x86-64 opcodes, ModRM/SIB, REX      backend_<arch> module;
  + register alloc      register set hardcoded              shared lowering strategy +
                                                        reg-alloc skeleton
                                                        parameterized by a
                                                        register-set description
  Calling convention    SysV: RDI/RSI/RDX/RCX/R8/R9,        abi_<target> module:
                        16-byte stack alignment, RAX        SysV (C-501), Win64
                        return                              (C-502), AAPCS64 (C-503),
                                                            wasm (native)
  Data layout           pointer/usize = 8 bytes assumed     Target descriptor (pointer
                        throughout layout code              width, alignment rules)
                                                        consumed by layout; wasm32
                                                        is the first 32-bit case
  Kavir IR              designed alongside two-operand      keep ops abstract; IR
                        x86-64 lowering; risk of x86-       snapshot tests stay
                        flavoured ops (div/mod pairs,       target-neutral; the wasm
                        128-bit return slots)               structured-control-flow
                                                        lowering is the designated
                                                        IR-purity stress test
                                                        (ADR-002 gate 3)
  Conformance/tests     harness runs Linux ELF binaries     per-target runner: CI
                                                        matrix (arm64, windows,
                                                        macos runners; wasmtime for
                                                        wasm); qemu-user optional
                                                        in the dev sandbox
  Reproducibility       byte-reproducible ELF (E4)          each target's acceptance
                        (C-708)                             includes byte-reproducible
                                                        output for its format
  Allocator growth      brk/mmap only                       sys_grow(bytes) in the sys
                                                        layer: brk (Linux),
                                                        memory.grow (wasm),
                                                        VirtualAlloc (Windows),
                                                        mmap via libSystem (macOS)
  Debug/line tables     ELF-oriented minimal line info      per format: name section
                                                        (wasm), none for PE at
                                                        first; no DWARF promise

Conclusion of the audit: with the seams above respected, each new target
is ADDITIVE — a new set of modules (encoder/isel, abi, sys, emit, runner)
plus a Target descriptor; the frontend, IR, checker, and tooling are
shared and unmodified in principle. The audit's discipline rule is what
keeps that true while v0.1.0 stays single-target.

## 3. Wasm ordering question (user question 4)

RECOMMENDATION: Wasm second (after Linux ARM64, before Windows/macOS) —
not first, not last.
- Why early: (a) cheapest verification of any non-x86 target — wasmtime
  runs on any CI/host, no hardware needed; (b) it preserves the
  freestanding no-libc philosophy exactly (WASI imports replace syscalls;
  no import tables, no dynamic linking, no codesigning) whereas Windows
  and macOS both force compromises; (c) it is the real test of ADR-002's
  "second backend by design" — a genuinely different code shape — and
  doing it while the codebase is ~10k LOC makes IR fixes cheap; (d)
  recorded hypothesis C-426 already flagged early Wasm commitment as a
  cheap cross-platform distribution/bootstrap lever (hypothesis status —
  to be re-evaluated with T3 results, not assumed).
- Why not before ARM64: ARM64 reuses the ELF+kernel posture and is the
  smallest delta on the native axis; carving the seams once with the
  conservative target first makes the radical target (wasm) cheaper and
  keeps v0.1.x momentum on one codegen shape. If raw reach mattered more
  than risk symmetry, wasm-first would be defensible; this ADR weights
  risk and seam-validation higher.

## 4. Scope guard

Nothing in this ADR changes v0.1.0 (ADR-005 v2 untouched). Each target
activation (T2..T5) is its own future decision with its own acceptance
criteria (byte-reproducible output, conformance suite green on that
target, honest docs). The v0.1.x obligation is only the discipline rule:
keep every platform coupling inside its per-target module.

Evidence IDs: C-411, C-413, C-426, C-501, C-502, C-503, C-505, C-516,
C-708
Local artifacts: decisions/ADR-002-implementation-strategy.md (seam +
gates), decisions/ADR-005-v0.1.0-scope.md (untouched scope),
benchmarks/e4-reproducibility.md

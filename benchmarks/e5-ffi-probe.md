# E5 — FFI probe results

Date: 2026-10-26. Pre-declared scope in `benchmarks/methodology.md`.
Artifacts: `e1-backend-probe/probe.ssa` (QBE leg) — this probe is embedded in
the E1 workload, and the E1 correctness gate (identical checksums) is also
this probe's pass criterion.

## What was exercised (QBE 1.2 → SysV amd64 → glibc)

- Calling libc `calloc`/`free` with 64-bit (`l`) arguments, four times per run.
- Calling variadic `printf` with a format string and a mixed vararg list
  (`l %count, l %sum, w %e0, w %e15`) — 64-bit and 32-bit varargs in one call.
- Passing a **function defined in QBE IL** (`$cmp`) as a callback to libc
  `qsort` — the reverse direction of interop (C code calling Kavir-side code
  through the C ABI), which is the harder and more important case for an FFI
  story.
- Result: linked and ran; output byte-identical to both gcc builds
  (`664579 3203324994356 9405696 4127412480`).

## What was NOT exercised (declared out of scope in methodology)

- Struct-by-value argument classification (INTEGER/SSE eightbyte rules).
- `va_list` consumption (`vastart`/`vaarg`) from the language side.
- `long double` / SIMD-class arguments.
- Other targets (AAPCS64, Win x64, RISC-V) — per evidence C-501..C-506 these
  need per-target work regardless of backend choice.

## Verdict

Smoke test PASSED for the candidate "small backend" path: QBE's C-ABI story
(scalar/pointer args, varargs, callbacks) worked on the first realistic
attempt, with all three interop directions (call into libc, variadic call,
called from libc). This is consistent with Phase 1 evidence that C-ABI-first
interop is the right v1 scope (C-114, C-120) and that per-target ABI layers
are unavoidable (C-501..C-506). A full ABI conformance suite remains Phase 3
work (per-target conformance pages, hypothesis C-519).

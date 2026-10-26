# Benchmark methodology

Write metrics, workloads and the result that would support or refute each
hypothesis BEFORE running anything. Record environment (CPU, OS, compiler
version, flags) with every result.

## Sandbox constraints (recorded 2026-10-26, before any run)

- Host: 2 vCPU Intel Xeon, Debian trixie container, Linux x86_64.
- Available: gcc/cc 14.2.0, make, python3. NOT available: clang/LLVM and
  rustc/cargo (no root to install, no preinstalled Rust).
- Consequence for probe E1: it compares **GCC vs QBE**, not the originally
  desired LLVM/Cranelift/QBE trio. The LLVM and Cranelift legs remain
  "unknown in this sandbox"; they are NOT estimated from blog numbers.
  QBE 1.2 is built from the official c9x.me release tarball.

## E1 — backend probe (pre-declared before running)

Hypotheses under test: C-425 (two-tier backend strategy) and the qualitative
claims C-405/C-413 (small backends trade peak code quality for compile speed).

Workload — identical algorithm in both representations, deterministic,
integer-only (no FP reassociation hazard):

1. Sieve of Eratosthenes to 10,000,000 (calloc'd byte array).
2. 512x512 uint32 matrix multiply (wrapping arithmetic, order-independent).
3. libc interop: calloc allocations, printf checksum, qsort with a comparison
   callback (a C function in the GCC leg, a QBE-defined function in the QBE
   leg — same signature).

Output: one checksum line. All binaries must print IDENTICAL output, else the
whole run is discarded and re-run after fixing.

Builds: `gcc -O0` (fast tier), `gcc -O2` (optimized tier), and
`qbe probe.ssa → cc` (assembly+link only; no C optimizer involved).

Metrics: compile wall time (median of 5), run wall time (median of 3,
includes ~1-2 ms exec overhead), binary size, checksum equality.

Pre-declared for/against criteria:

- Two-tier hypothesis SUPPORTED if gcc -O0 compiles >=5x faster than gcc -O2
  AND runs >=2x slower than gcc -O2 (the tier trade-off is real and large).
- QBE datapoint SUPPORTS "a small backend buys most of the speedup cheaply"
  if QBE compile time <= 2x gcc -O0's AND QBE run time >= 20% faster than -O0;
  otherwise it is recorded against, with the numbers shown.
- Declared limitation: the source is ~80 lines, so compile-time medians are
  dominated by fixed driver overhead. Compile time here is a sanity datapoint,
  not a scaling claim; run time and binary size are the primary metrics.

## E4 — reproducibility probe (pre-declared)

Hypothesis: C-708/C-709 — a lockfile alone does not give bit-reproducible
builds; environment leaks break reproducibility and need explicit control.

Procedure (same probe.c):
(a) same directory, plain `cc -O2`, built twice -> compare sha256;
(b) `cc -O2 -g` from two different directories -> compare;
(c) repeat (b) with `-ffile-prefix-map=<dir>=/usr/src/kavir-probe` -> compare.

For (hypothesis confirmed): (a) identical, (b) different, (c) identical.
Refuted if any step disagrees. Note: probe.c uses no __DATE__/__TIME__/__FILE__,
so the date/macro hazards stay documented (C-708) rather than exercised.

## E5 — FFI probe (pre-declared)

Scope: smoke test of C-ABI interop through each backend: call calloc/qsort/
printf from the program; pass a callback (QBE-defined `$cmp`) to libc qsort.
NOT an ABI conformance suite: struct-by-value classification, va_list and
long double are out of scope here (covered by evidence C-501..C-506).
For: the QBE leg links against libc and prints the identical checksum.

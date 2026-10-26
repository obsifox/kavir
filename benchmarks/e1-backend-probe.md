# E1 — backend probe results

Date: 2026-10-26. Methodology and for/against criteria were pre-declared in
`benchmarks/methodology.md` BEFORE any run. Harness: `run_e1.py` (in this
directory); sources: `probe.c` (GCC leg), `probe.ssa` (QBE leg, hand-written
QBE IL implementing the identical algorithm).

## Environment

- 2 vCPU Intel Xeon, Debian trixie container, x86_64, Linux.
- gcc (Debian) 14.2.0; QBE 1.2 built from the official c9x.me release tarball.
- clang/LLVM and Rust/Cranelift unavailable in this sandbox (no root, no
  rustc) — the planned LLVM/Cranelift legs are NOT run and NOT estimated.

## Correctness gate (pre-declared: discard if checksums differ)

All three binaries printed the identical checksum line:

`664579 3203324994356 9405696 4127412480`

(664,579 primes below 10^7 — matches the known value; u64 prime sum; u32
corner checksum after qsort.) The QBE leg also exercised libc interop:
calloc/free, variadic printf, and qsort with a QBE-defined callback (`$cmp`) —
see `benchmarks/e5-ffi-probe.md`.

## Results (medians; all raw runs kept in the harness output)

| build   | compile ms | run ms | binary bytes |
|---------|-----------:|-------:|-------------:|
| gcc -O0 |       30.3 |  463.1 |       16,136 |
| gcc -O2 |       49.0 |   96.7 |       16,136 |
| qbe+cc  |       18.4 |  254.2 |       16,176 |

- gcc -O2 vs -O0 run time: 4.79x faster (366.4 ms saved).
- QBE vs -O0 run time: 45.1% faster (209 ms saved) = 57% of the full -O0→-O2
  improvement, at LOWER compile time than -O0 (18.4 ms vs 30.3 ms).
- QBE vs -O2 run time: 2.63x slower.

## Verdict against pre-declared criteria

1. **Two-tier hypothesis (C-425): runtime half SUPPORTED, compile-time half
   UNMEASURABLE at this scale.** The run-time tier gap is real and large
   (4.79x >= 2x required). The compile-time criterion (>=5x) was NOT met:
   1.62x on an ~80-line file, where fixed driver overhead dominates — this
   limitation was declared up front, so it counts as "not measurable here",
   not as refutation. A scaling version (multi-file, generated workload) is
   deferred to Phase 3.
2. **QBE datapoint: SUPPORTED.** Both pre-declared conditions met: compile
   time 0.61x of gcc -O0 (<= 2x required) and run time 45% faster than -O0
   (>= 20% required). On this scalar-heavy workload a tiny backend (~15k LOC
   project) delivered more than half of the optimizing tier's win.

## Honest caveats (recorded, not hidden)

- Single workload, scalar/integer heavy. gcc -O2 vectorizes the matmul
  (SIMD); QBE does no vectorization — the 2.63x residual gap vs -O2 is
  expected to be dominated by that single optimization class, which is
  exactly the "peak code quality" part of the C-405/C-413 trade.
- Compile times at this file size measure driver fixed costs more than
  backend complexity; they are a sanity datapoint only.
- The QBE leg hand-writes what a Kavir compiler would generate; writing it
  took one focused session including two SSA/phi authoring mistakes that the
  QBE frontend caught immediately (phi predecessor mismatch; temp type
  collision) plus one 1.2 parser quirk (empty variadic tail `...)` rejected).
  Authoring-quality observations, kept as anecdotes, not claims.
- This does NOT substitute for the still-unknown independent
  Cranelift-vs-LLVM code quality comparison (feasibility-report section 9).

## Implication for the implementation-strategy ADR (ADR-002)

The local datapoint is consistent with the researched evidence: a small
SSA backend gives a large share of the practical win at negligible compile
cost, while the optimizing tier remains the only path to peak performance.
This supports a two-tier or QBE-first backend strategy as the default
candidate for v0.1.0, with LLVM/Cranelift as later tiers if needed.

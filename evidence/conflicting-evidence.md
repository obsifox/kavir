# Conflicting evidence

Record each disagreement: claim ID(s), the sources on each side, and what would resolve it.

## 1. "70% of vulnerabilities are memory safety" — universal law vs. per-denominator statistic

- Side A (headline numbers): S-111 (MSRC ~70% of CVE-assigned), S-112 (Chromium ~70% of
  high-severity), S-114 (Android 76%→24%).
- Side B (methodology criticism): S-129 — denominators differ (CVEs assigned vs.
  high+critical browser bugs vs. platform vulnerabilities); survivorship bias; do not
  quote 70% as a share of all vulnerabilities everywhere.
- Claims involved: C-104, C-105, C-106, C-107, C-606, C-607.
- What would resolve it: per-codebase replication with published denominators and time
  windows; treat vendor numbers as internal observations only (handled by C-107).

## 2. Zig production readiness

- Side A: S-121 (official: small static binaries, libc independence; also unsourced
  "Zig is faster than C" benchmark claim — red flag per source-quality.md).
- Side B: S-122 (production use is real — TigerBeetle/Ghostty/OpenTelemetry injector —
  but pre-1.0, over-explicit conversions; Bun rewrote its runtime to Rust).
- Claims involved: C-118.
- What would resolve it: independent benchmarks with method + hardware + version, and
  a Zig 1.0 stability release.

## 3. Formal status of Rust's unsafe semantics

- Side A: S-105/S-602 (RustBelt: machine-checked soundness of a realistic subset).
- Side B: S-101 (Reference: no formal model of what is allowed in unsafe; UB list
  non-exhaustive and may change).
- Claims involved: C-109, C-110, C-602.
- What would resolve it: distinguish normative-reference scope from research semantics;
  verify RustBelt coverage against the current compiler version before citing Rust as
  "formally proven" end to end.

## 4. Bun's Zig→Rust rewrite attribution

- S-122 reports the rewrite was for maintainability of a concurrent garbage-collected
  runtime, not language stability; popular commentary often attributes it to Zig
  language limits.
- Claims involved: C-118 (notes).
- What would resolve it: fetch Bun's own 1.4 announcement (primary) before using this
  case anywhere.

## 5. Cranelift undefined behavior claim (status: conflicting, claim C-407)

- Side A: S-405 (cranelift.dev: "our IR does not have undefined behavior, by design").
- Side B: S-407 (Cranelift IR reference: loads/stores with `accessible` flag have
  undefined behavior when memory is not accessible).
- Same origin group (Bytecode Alliance); the normative reference nuances the marketing
  page.
- What would resolve it: treat ir.md as authoritative; verify each flag's semantics
  before relying on "no UB" for any backend decision.

## 6. Should a language adopt LLVM?

- Side A (no): S-419 — Go's designers rejected LLVM as too large and slow and
  ABI-limiting (C-419).
- Side B (yes): S-414, S-415 — Swift/Rust layer language-specific IRs over LLVM
  successfully and document workarounds rather than regret (C-414, C-415).
- What would resolve it: Blup's own goal definition (compile-speed targets, runtime
  model, team size) — this is a Phase 2 ADR, not a research question with a universal
  answer. Related numbers: C-405 (codegen ~10x faster but ~14% slower code) vs C-417
  (20-30% faster clean debug builds) — do not conflate code quality with compile time.

## 7. Checked exceptions

- Side A (pro): S-626 — JLS §11.2 recommends checked exceptions to exploit compile-time
  checking (C-610).
- Side B (contra): S-608 — C# lead designer: scalability and versionability problems
  (C-611).
- What would resolve it: no controlled study found either way; note that result-type
  languages (Go, Rust) chose explicit values with different boilerplate costs (C-612).
  Unresolved design conflict; Phase 2 ADR with an ergonomics experiment.

## 8. Message passing vs shared memory safety

- Side A: Go's design advice implies channels make inter-thread communication safer
  (S-603, S-609).
- Side B: ASPLOS'19 empirical study — message passing as bug-prone as shared memory;
  ~58% of blocking bugs from channels (C-619).
- Resolution: empirical study wins for bug prevention; channels do not prevent
  concurrency bugs by construction (C-618 detection-only).

## 9. GC pause risk

- Side A: S-614, S-615 — ZGC sub-ms to ≤10ms pauses not scaling with heap (C-613).
- Side B: real-time/latency criticism of GC — could not be fetched this session; JEP
  numbers are single-vendor benchmarks with stated variance.
- What would resolve it: independent replication; strict-real-time suitability remains
  unknown — flagged, not assumed.

## 10. async/await value

- Side A: S-611 — many tasks on few threads, better than thread pools for I/O (C-615).
- Side B: S-612, S-625 — lifetimes/pinning complexity, async struggles common; Rust
  survey: compile times top limiter (C-615, C-616).
- Resolution: both from Rust project origins — benefit real, ergonomic cost real.
  Nystrom's "function coloring" criticism (S-628) remains an unfetched lead.

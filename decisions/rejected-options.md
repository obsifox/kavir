# Rejected options

One line per rejected option: what, why, evidence IDs, date, where recorded.
Rejected ideas come back — this file is the memory that they were already
considered.

- Claim superiority over C/C++/Rust/Go without measured benchmarks — no
  evidence supports it at 0.x and the charter forbids unmeasured claims;
  C-104..C-107, C-718..C-727 — 2026-10-26 (ADR-001).
- General-purpose "replace X" positioning — adoption evidence requires a
  killer use case, sponsor or interop path we do not have; C-718..C-727 —
  2026-10-26 (ADR-001).
- Managed VM / hosted runtime for v0.1.0 — foreign runtime, JIT warmup and
  AOT closed-world costs documented by both vendors; C-201..C-210 —
  2026-10-26 (ADR-002, proposed).
- Custom optimizing backend from scratch — Go's own history shows it needs
  institutional funding and years; C-419/C-420 — 2026-10-26 (ADR-002 v1).
  SUPERSEDED same day: the user explicitly CHOSE this path with binding
  constraints (direct machine code, no cc, no libc) — accepted risk, see
  ADR-002 v2; the thin IR and falsification gates are the mitigations.
- QBE-first via cc (original ADR-002 recommendation) — every produced
  binary would pass through a C toolchain path, excluded by user constraint;
  C-413, C-405 — 2026-10-26 (ADR-002 v2, user decision).
- Assembly text + GNU binutils as/ld as the v0.1.0 build path — still an
  external-toolchain dependency and target-default coupling (C-517);
  retained ONLY as an explicit fallback requiring a fresh user decision if
  the ELF/ABI work stalls (ADR-002 falsification gate 2) — 2026-10-26
  (ADR-002 v2).
- libc linkage in compiled programs (stdio/malloc/sanitizers via libc) —
  user constraint: direct syscalls only, freestanding runtime (kavirrt);
  C-516 — 2026-10-26 (ADR-002 v2). Consequence: C-library FFI is out of
  v0.1.0 scope.
- Tracing GC in v0.1.0 — runtime weight contradicts AOT-native minimal
  runtime; pause claims are vendor-single-sided (conflict #9); C-613, C-211,
  C-212 — 2026-10-26 (ADR-003, proposed).
- Manual-memory-only core — forfeits the largest CVE class; C-104 —
  2026-10-26 (ADR-003, proposed).
- Gradual/dynamic-first typing with soundness retrofits — the decade-scale
  retrofit pattern the evidence base warns about; C-301, C-302, C-305,
  C-307 — 2026-10-26 (ADR-004, proposed).
- Broad-surface v0.1.0 (generics+concurrency+registry+async at once) — the
  never-ships pattern (Perl 6 / Crystal / Julia pre-1.0 limbo); C-715,
  C-722..C-725 — 2026-10-26 (ADR-005, proposed).
- ADR-005 v1 fixed-bundle order (vertical slice defined as a feature bundle
  without an end-to-end-first ladder) — REJECTED by the user: milestone
  order must be a walking skeleton (M1 = return 42 lexer→ELF) —
  2026-10-26 (ADR-005 v2).
- Lifetime inference (NLL-class), borrow returns, and struct-field borrows
  in v0.1.0 — checker complexity the slice cannot carry; deferred to
  post-0.1 via the ADR-003 subset/cut list; C-620 — 2026-10-26.
- Traits/bounds and operator overloading in v0.1.0 — the generics dilemma
  has no free option; v0.1.0 buys the simplest monomorphizing corner;
  C-308/C-309 — 2026-10-26 (ADR-004 subset).

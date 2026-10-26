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
  institutional funding and years; C-419/C-420 — 2026-10-26 (ADR-002,
  proposed).
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

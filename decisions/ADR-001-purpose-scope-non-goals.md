# ADR-001: Purpose, target users and non-goals

Status: accepted (grounds: feasibility gate approved by the user on 2026-10-26;
this ADR formalizes the approved recommendation)
Date: 2026-10-26
Question: Why must a new language exist for this, who is it for, and what is
explicitly out of scope — including whether "replace C/C++/Java" claims are in
scope?

Options considered (at least two):

  A. Position Kavir as a general-purpose replacement for C/C++/Rust/Go and
     claim superiority. Evidence: C-104..C-107 (memory-unsafety CVE burden is
     real); countered by C-718..C-727 (durable adoption needs a killer use
     case, sponsor or interop path) and conflict #6 (LLVM-or-not resolves only
     against goals we never declared).
  B. Position Kavir as an experimental, narrow-scope language whose v0.1.0 is
     learning-oriented and honest: a small core with machine-checkable
     semantics ambitions, an explicit unsafe boundary, race prevention by
     construction with a detector backstop, integrity-first tooling, and
     denominator-stated safety claims. Evidence: C-109 (RustBelt shows a small
     core can be formalized), C-101 (open-ended UB is the anti-pattern),
     C-617/C-618, C-730, C-107.
  C. Do not build a language; contribute to an existing one or ship a library.
     Evidence: feasibility-report section 5/10 — honest for anyone whose goal
     is a production replacement today.

Trade-offs: Option A maximizes ambition but is contradicted by the adoption
evidence and by the charter's no-unmeasured-claims rule. Option C maximizes
prudence but ignores the user's explicit goal (charter: build a new language
to v0.1.0). Option B matches the user-approved feasibility recommendation and
keeps every claim falsifiable.

Decision and reasoning: Option B, with Option C recorded as the standing
fallback if Phase 3 shows the vertical slice cannot be made honest and
working. Target users (v0.x): the maintainer (learning-by-building), then
early experimenters comfortable with experimental software. Non-goals:
replacing any existing language; production readiness claims at 0.x; compile-
time or run-time superiority claims without measured, environment-stated
benchmarks; a batteries-included stdlib (C-716); enterprise governance features.

Reversibility (cheap / costly / one-way): cheap — positioning can be revised
while the repository is private; becomes costly after public release and any
measured claim is published.

What result would show this was wrong (experiment or metric): if the Phase 3
prototype cannot reach an honest, tested vertical slice within the v0.1.0
scope (ADR-005), or if no user-facing value slot from feasibility-report
section 5 survives contact with the prototype, fall back to Option C.

Evidence IDs: C-101, C-104, C-105, C-106, C-107, C-109, C-617, C-618, C-716,
C-718, C-727, C-730

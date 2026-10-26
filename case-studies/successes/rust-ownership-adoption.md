# Rust's ownership model — from unproven claims to checked semantics and industrial adoption (2015–2026)

Period and context:
2015 (Rust 1.0) to 2026. Rust's safety claims were initially informal; formal
verification (RustBelt, POPL 2018) and industrial adoption (Android, Linux kernel)
proceeded in parallel.

Problem it was meant to solve (C-ids):
Systems programming with manual memory management produced the dominant bug classes
(C-104, C-105). Rust's answer is ownership/borrowing with no garbage collector
(S-102, C-601) and UB forbidden even under unsafe, with a soundness duty on the
programmer (C-108).

What worked in practice (C-ids):
Machine-checked semantic soundness proof for a realistic subset including unsafe
library patterns (C-109, C-602). The safe-abstraction pattern works in production
kernels via core-only builds and C bindings (C-120). Measured outcome: Android's
memory-safety share collapse where Rust was adopted in new code (C-106, C-607).

Costs and new problems (C-ids):
Learning curve is a documented adoption barrier (C-112). Async Rust is officially
harder with higher maintenance burden and runtime fragmentation (C-113). Real-world
bugs still occur in unsafe code and safe-to-unsafe chains (C-111, C-603). The
normative Reference admits no complete formal unsafe model and a non-exhaustive UB
list (C-110) — guarantees rest on a moving document plus evolving research semantics.

Valid under which conditions:
Bug classes limited to memory/UB/races (ownership helps little for logic bugs);
teams able to absorb lifetime-reasoning training; domains tolerant of compile-time
friction; mature tooling (cargo, clippy).

Alternatives that reach the same goal:
Tracing GC (different cost profile, C-613); ARC with cycle-handling discipline
(C-608); SPARK-style contracts and proof for high-assurance niches (C-119).

Causes of limited adoption (if any), split:
technical: compile times and lifetime friction (C-616, C-620); ecosystem: smaller
library pool in some niches; governance/timing: pre-2015 tooling immaturity.

Counter-evidence and disagreements:
Adoption-barrier scholarship (S-107, S-108) shows the ergonomic cost is systemic,
not anecdotal. The Reference's "no formal model" statement (S-101) coexists with
RustBelt's formal semantics (S-105) — the proof covers a subset at a semantic
level, not the whole normative language (recorded in conflicting-evidence.md).

Confidence: high
(for the model's guarantees and formal proof, primary sources; medium for the claim
that these guarantees causally produced Android's decline, vendor attribution)

Open questions:
How much of the learning-curve cost is removable by better diagnostics (NLL was a
major redesign, C-620)? What is the measurable onboarding gap vs. GC languages?

Implications for our design (as hypotheses only):
H4: keep Blup's core semantics small enough that machine-checked semantics stays
feasible (C-101's open-ended UB is the failure mode to avoid; C-117's explicit
illegal-behaviour catalogue is a middle path).
H5: an explicit, auditable unsafe boundary with a soundness definition belongs in
v1, not retrofitted.
H6: budget the concurrency/async story early — retrofitting async into Rust
produced fragmentation (C-113).
H7: diagnostics and inference quality are a first-class design constraint because
learning cost measurably gates adoption (C-112).

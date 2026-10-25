# Ownership adoption in memory-safe system code (Rust studies + Android shift)

Period and context:
PLDI 2020 study of real Rust applications/libraries; Android platform shift
2019–2022 (S-619, S-624); RustBelt formalization completed 2018 (S-602).

Problem it was meant to solve (C-ids):
Memory unsafety dominated serious security bugs (~70% of Chromium serious bugs,
C-606); C/C++ new code kept producing UAF/races (C-607).

What worked in practice (C-ids):
Ownership rules remove use-after-free/double-free classes statically with no GC
(C-601); the model survived machine-checked formalization including
unsafe-library conditions (C-602); Android's new-code shift coincided with the
memory-safety vulnerability share dropping 76% to 35% by 2022 (C-607) and 24% by
2024 (C-106).

Costs and new problems (C-ids):
All memory-safety bugs found in real Rust still involve unsafe code (C-603);
lifetime reasoning is hard and error-prone (C-620); compile times top the
survey's productivity complaints (C-616); ownership is a new concept that takes
time to learn (C-601 notes).

Valid under which conditions:
New code must dominate the security-sensitive surface; unsafe must stay isolated
and reviewed; ecosystem/tooling must be mature.

Alternatives that reach the same goal:
ARC (C-608); GC with low-latency collectors (C-613); heavy sandboxing plus
hardened allocators (S-112, S-113).

Causes of limited adoption (if any), split:
technical: lifetime friction, compile times; ecosystem: async runtime
fragmentation (C-113, C-615); governance/timing: pre-1.0 churn ended 2015.

Counter-evidence and disagreements:
Safe code still participates in most memory bugs via safe-unsafe chains (C-603
notes); async Rust reintroduces complexity (C-615, C-616); Android authors
themselves warn against causal over-reading (C-607).

Confidence: high
(two scholarly studies + two independent vendor origin groups)

Open questions:
Does the 58%-of-blocking-bugs-from-message-passing style result (C-619) have a
Rust-side equivalent for async? Region/arena literature remains unfetched (S-630).

Implications for our design (as hypotheses only):
Blup should make the unsafe surface syntactically explicit and small; an
ownership core with optional managed layer is plausible but unproven (C-623);
invest in lifetime-diagnostics tooling from day one (C-620).

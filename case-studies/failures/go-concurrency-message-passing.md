# Go's goroutines+channels data-race story

Period and context:
Go 2009–2019 study period; memory model revised 2022; race detector shipped 2013
(S-603, S-618, S-622).

Problem it was meant to solve (C-ids):
Does message passing (CSP style) prevent concurrency bugs? Go's model made racy
execution entirely undefined (C-604) with no static prevention.

What worked in practice (C-ids):
Cheap goroutines plus a simple model scaled well; an integrated dynamic race
detector (TSan-based) gives a practical backstop (C-618); the memory model was
tightened with happens-before in 2022 (C-604).

Costs and new problems (C-ids):
171 bugs studied in six real projects; message passing as bug-prone as shared
memory, ~58% of blocking bugs caused by channels (C-619); prevention is left to
runtime tooling, not the type system (C-618).

Valid under which conditions:
Teams run race detectors in CI; channels used with discipline; performance
permits a GC runtime.

Alternatives that reach the same goal:
Ownership-based static prevention (Send/Sync, C-617); structured concurrency
with scoped tasks (C-622); virtual threads for blocking-style scaling (C-614).

Causes of limited adoption (if any): n/a (mainstream language).

Counter-evidence and disagreements:
Go's own documentation advocates message passing for safe inter-thread
communication (S-603 advice, S-609/S-610 style) vs. the ASPLOS'19 empirical
result (C-619); the empirical study wins for bug prevention. CSP advocates' own
successor work argues unstructured spawn itself is the problem (C-622).
Recorded in conflicting-evidence.md.

Confidence: high
(scholarly study + official docs from independent origins)

Open questions:
Do structured-concurrency designs measurably reduce bug rates (no controlled
study found this session)? What is the ergonomic cost of static race prevention
outside Rust's specific design?

Implications for our design (as hypotheses only):
Blup should prefer prevention-by-construction (ownership transfer / Send-like
traits, C-617) with a race detector as backstop (C-618); structured scoped
spawning deserves first-class status (C-622, needs validation).

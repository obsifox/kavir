# Go generics: the eight-year delay (2009–2022)

Period and context:
Go launched 2009 (S-311); "where are the generics" was one of the most frequent
questions (S-312); type parameters accepted 2021, shipped in Go 1.18, early 2022
(S-313).

Problem it was meant to solve (C-ids):
C-309 — the generic dilemma (S-312): C-style absence slows programmers; C++-style
templates slow compilers and bloat binaries; Java-style boxing slows execution.
Go prioritized fast builds, readability and maintainability (S-311).

What worked in practice (C-ids):
Deferring the feature kept the language simple and tool-friendly during growth
(C-308); a long design iteration (interfaces as constraints, S-313) landed a
design the team judges worth its complexity (C-308).

Costs and new problems (C-ids):
A decade of duplicated container/utility code for users; community pressure;
the compiler now chooses between per-instantiation and shared-shape
implementations (S-310), adding implementation complexity.

Valid under which conditions:
Large teams with mixed skill levels; server-side workloads where compile speed
and binary simplicity dominated (S-311); a funded team able to run multi-year
design iterations (S-313) — resources Kavir will not have.

Alternatives that reach the same goal:
Ship generics at 1.0 with a chosen compilation strategy (per-instantiation,
shared-shape, or both tiered); macro systems; code generation tools.

Counter-evidence and disagreements:
Go's own FAQ now says generics come at a cost in complexity in the type system
and run-time (S-310) — the omission also had real costs; no independent study of
the delay's productivity impact was found.

Confidence: medium (key sources are Go-origin testimony; the dilemma framing
is interpretation).

Open questions:
How do per-instantiation vs shared-shape implementations compare on compile
time and binary size at scale? (Go's own docs do not publish the trade.)

Implications for our design (as hypotheses only):
Omissions are a legitimate simplicity strategy, but a new language should decide
generics and their compilation strategy BEFORE 1.0, because retrofit took the
Go team about eight years even with resources Kavir will not have.

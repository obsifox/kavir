# Gradual typing adoption (TypeScript 2012–, Python type hints 2014–)

Period and context:
Dynamic languages (JS, Python) became dominant for large applications; teams
needed structure without abandoning dynamic code. TS ships types as erasable JS
annotations; Python standardizes hints in PEP 484 (Python 3.5).

Problem it was meant to solve (C-ids):
Dynamic typing hinders comprehension and refactoring at scale (C-304; PEP 484's
own rationale, S-302).

What worked in practice (C-ids):
Optional, tool-enforced annotations with zero runtime behavior — adoption was
cheap because existing tooling kept working (C-301, C-319); types as IDE/
completion/refactoring substrate (S-302); code quality/understandability gains
measured in one large study (C-306).

Costs and new problems (C-ids):
Soundness was NOT fixed — TS declares it a non-goal (C-302) and documents
unsound holes (C-303); Python has no runtime enforcement (C-301) and spent about
a decade on annotation-evaluation semantics (C-307); sound alternatives pay
runtime-cast overhead up to 100x (C-305); the type-checker becomes a central
tool dependency (Dropbox employs mypy core team members, S-320).

Valid under which conditions:
Large, long-lived, multi-developer codebases; gradual migration paths.

Alternatives that reach the same goal:
Fully static core languages (different trade-off); runtime-enforced gradual
typing (sound but slow, C-305); Elixir's "strong arrows" research direction
(S-328).

Causes of limited adoption (if any), split: technical — unsoundness holes and
cast costs; ecosystem — checker quality dependency; timing — pre-2012 tooling
immaturity.

Counter-evidence and disagreements:
C-306 (no significant bug-rate improvement for TS over JS); S-328 (Valim:
gradual typing's benefit is "true and false at the same time"). See
conflicting-evidence.md #13-14.

Confidence: high for what gradual typing is and does; medium for outcome
claims (single studies).

Open questions:
Do sound gradual-typing overheads become tolerable with modern JIT techniques?
No post-2018 measurement was fetched.

Implications for our design (as hypotheses only):
H (C-320): pick the soundness contract deliberately in Phase 2; do not promise
runtime guarantees the checker does not enforce; treat annotation evaluation
semantics as a language-core decision, not an afterthought.

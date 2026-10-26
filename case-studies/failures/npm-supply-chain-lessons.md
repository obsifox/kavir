# left-pad, event-stream, and what lockfiles actually buy

Period and context:
npm registry, March 2016 (left-pad) and Oct–Nov 2018 (event-stream); Go answered
in 2019 with the checksum database; PyPI institutionalized external audits by
2023.

Problem it was meant to solve (C-ids):
Left-pad: one author's unpublishing of 273 packages broke thousands of builds
for 2.5 hours (C-710). Event-stream: a maintainer-trust transfer shipped malware
downloaded ~8M times undetected for 2.5 months (C-711). Both are registry/trust
failures, not compiler or tool bugs.

What worked in practice (C-ids):
npm's postmortem led to safeguards restricting unpublish/republish (C-710); Go
made a mirror + transparency-log checksum database the default (C-705); PyPI
published a full external audit with 29 advisories and no high-severity findings
(C-717); exact-tree lockfiles became standard (C-702, C-706, C-703).

Costs and new problems (C-ids):
Lockfiles pin the dependency graph but not the toolchain/environment, so they
are not bit-reproducibility (C-709); bit-reproducibility additionally demands
controlled environments per the canonical definition (C-708) — a real
engineering investment.

Valid under which conditions:
Integrity mechanisms matter most when a central registry is the default install
path for beginners — exactly the ecosystem Kavir would be building.

Alternatives that reach the same goal:
Fully vendored dependencies; distro-style curation; no central registry at all
(each with its own adoption costs).

Causes of limited adoption (if any): n/a (mechanisms adopted industry-wide).

Counter-evidence and disagreements:
Some in the community attributed left-pad's severity to npm's micro-package
culture rather than registry policy (S-727 acknowledges falsehoods circulated;
the founder's own post is now a dead link, S-734). Both sides agree on the
facts (C-710) but not the cause — see conflicting-evidence.md #16.

Confidence: facts high/medium (C-710 high, C-711 medium); interpretation
unverified.

Open questions:
What unpublish/republish policy balances security response with the left-pad
lesson? (Design question for Phase 2; no empirical answer found.)

Implications for our design (as hypotheses only):
H (C-730): build checksum verification, un-publish policy, and lockfile
semantics into the package-manager spec v0; document explicitly what the
lockfile does and does not guarantee (C-709).

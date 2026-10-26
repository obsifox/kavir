# RISC-V psABI evolution (an ABI designed while hardware is still moving)

Period and context:
2019–2026. RISC-V ratified RVWMO (2019) and V (2021) while the psABI stayed a
living draft (master is 2.0-rc0, "dev"); extension cadence forced the ABI to
name variants (ILP32/ILP32F/ILP32D/ILP32E/LP64...).

Problem it was meant to solve (C-ids):
C-504, C-512, C-513: aggregates, unions, variadics and stack alignment had to
be pinned per ISA variant; ILP32E explicitly not guaranteed stable; naive
atomics mappings break seq_cst between languages (C-513).

What worked in practice (C-ids):
Layered naming of ABI variants by extension set (C-504); explicit instability
warnings instead of silent divergence; a separate Atomics ABI document binding
C/C++/Java mappings to instructions (C-513).

Costs and new problems (C-ids):
Downstream toolchains must encode variant matrices; ecosystem fragmentation
risk while the "dev" draft circulates; ILP32E instability is exactly the kind of
moving floor FFI code trips on (C-504).

Valid under which conditions:
Typical of a young ISA ecosystem where extensions arrive faster than deployment
stabilizes; contrasts with AAPCS64 2025Q4, which absorbed SME/ZA rules while
keeping the core (SP mod 16, HFA/HVA) stable — mature ABIs can evolve
compatibly once the core is frozen (C-503).

Alternatives that reach the same goal:
Freeze v1 ABI strictly and version extensions (AAPCS64 path, C-503); never
promise ABI stability (web-language path).

Counter-evidence and disagreements:
AAPCS64's revision history shows 2021-era corrections handled as clarifications
without breaking the standard (S-509) — evolution without rupture is possible;
the fragmentation-risk framing is interpretation from a single origin group.

Confidence: high on the document states; the fragmentation-risk framing is
interpretation-grade.

Open questions:
How many ABI variants can a small language team actually test? (CI matrix cost,
C-517 context.)

Implications for our design (as hypotheses only):
Version the Kavir ABI explicitly, publish an ABI conformance page per target
from day one (like the RISC-V variant naming), and freeze the core calling
convention early while leaving extension-specific appendices open (feeds C-519).

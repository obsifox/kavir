# The vendor memory-safety pivot (Microsoft, Chromium, Android — 2019–2026)

Period and context:
2019–2026. Three of the world's largest C/C++ codebases (Windows ecosystem, Chrome,
Android) published internal vulnerability statistics and launched memory-safety
strategies that favor new code in memory-safe languages over rewrites.

Problem it was meant to solve (C-ids):
Dominant share of serious security bugs caused by memory errors in C/C++:
MSRC ~70% of CVE-assigned vulnerabilities (C-104); Chromium ~70% of high-severity
bugs with half use-after-free (C-105); Chromium concluded sandboxing/process
isolation had reached cost limits (S-112).

What worked in practice (C-ids):
Shifting new development to memory-safe languages instead of rewriting old code:
Android's memory-safety share fell 76% to 24% over six years (C-106, C-607);
vulnerability-decay model — most bugs are introduced in new code and code gets
safer with age (S-114). Retrofitting helped too: MiraclePtr mitigated a majority
of UAF in privileged Chrome processes (S-113).

Costs and new problems (C-ids):
Interop is the central cost: bridges between C++ and the safe language are the
recurring expense in every observed adoption (C-107, C-116). Denominator traps:
the 70% figures are not interchangeable and exclude undiscovered bugs (C-107).

Valid under which conditions:
Codebase where vulnerabilities concentrate in recent changes; funding for interop
tooling (Crubit, autocxx); old C/C++ kept maintained while it ages.

Alternatives that reach the same goal:
Sandboxing and site isolation (assessed as insufficient alone by Chromium, S-112);
sanitizer tooling and hardened allocators (MiraclePtr path, S-113).

Causes of limited adoption (if any): n/a (adoption succeeded).

Counter-evidence and disagreements:
Safeguard's methodology review (S-129) warns the shares differ by denominator and
time window and survivorship bias means fixed-bug counts say nothing about unknown
bugs (C-107). Each vendor counts its own CVEs; selection effects possible.

Confidence: high
(for the statistics as vendor-reported observations; medium for causal attribution
of the decline purely to language mix — vendor's own model, plausible confounders)

Open questions:
How do these shares look for non-browser, non-mobile codebases? No independent
(non-vendor) replication of the decay model was found this session.

Implications for our design (as hypotheses only):
H1: Blup should make memory safety the default for new code and treat incremental
adoption (C-ABI interop first) as the adoption path, not migration rewrites.
H2: Blup's interop surface must be designed before v1 because bridges are the
recurring cost across every observed adoption case.
H3: publish any safety claim with a stated denominator (learn from the statistics
criticism, C-107).

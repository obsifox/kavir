# Go's custom backend decision

Period and context:
2007–2018. Go's gc toolchain was written in C, converted to Go in 1.5 (2015),
gained a custom SSA backend in 1.7 (2016) and full-architecture SSA in 1.8 (2017)
(S-419, S-420, S-421, S-426).

Problem it was meant to solve (C-ids):
Fast compilation and tight control over the runtime's non-standard needs —
growable goroutine stacks, custom ABI — that a general backend like LLVM does not
expose cleanly (C-419).

What worked in practice (C-ids):
The custom SSA backend delivered 5-35% CPU-time improvements over the previous
(non-SSA) backend with more compact code (C-420); gc generates ELF/Mach-O/PE via
its own Plan 9-derived loader, no external assembler/linker dependency for basic
flows (S-426, C-420).

Costs and new problems (C-ids):
The project bore the full multi-year cost of rewriting its backend to SSA and
maintaining per-architecture rules (C-420); peak generated-code quality vs.
LLVM-based toolchains was never demonstrated as the goal, only user-facing
compile speed and runtime control (S-419).

Valid under which conditions:
Viable when the language needs non-standard execution semantics (stacks,
scheduling) and has institutional funding for a permanent compiler team; Go's FAQ
frames LLVM size/speed as disqualifying for their goals, not universally (C-419).

Alternatives that reach the same goal:
LLVM + language IR layering (Swift SIL, Rust MIR — C-414, C-415); lighter
backends (Cranelift, QBE — C-405, C-413); gccgo's experimental LLVM backend
shows the same language can use LLVM with different goals (S-419).

Causes of limited adoption (if any): n/a (mainline toolchain).

Counter-evidence and disagreements:
Swift and Rust adopted LLVM + language-specific IRs successfully and document
workarounds rather than regret (C-414, C-415) — origin groups disagree on whether
LLVM's cost is acceptable; the disagreement is recorded in
conflicting-evidence.md. Resolution requires Blup's own goal definition (Phase 2).

Confidence: high for facts; medium for the interpretation that Go's stack/ABI
needs were decisive (single-origin designer testimony, consistent with the
documented goroutine stack design).

Open questions:
Independent Cranelift-vs-LLVM and GCC-vs-LLVM code-quality benchmarks were not
found this session (treated as unknown).

Implications for our design (as hypotheses only):
If Blup wants ordinary native semantics, LLVM/Cranelift reuse looks cheaper than
Go's path; if Blup needs unusual stack or ABI control, budget for a custom lower
layer regardless (C-419, C-425).

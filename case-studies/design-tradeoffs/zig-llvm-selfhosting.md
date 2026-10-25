# Zig's LLVM dependency and self-hosting journey

Period and context:
2019–2025. Zig used LLVM for optimized codegen while building a self-hosted Zig
compiler; by late 2022 it carried two compilers: 80k lines of C++ (stage1) plus
~250k lines of Zig (S-422). Zig 0.11 notes document bringing self-hosted x86
codegen and incremental compilation to usable state (S-425).

Problem it was meant to solve (C-ids):
stage1 was required to build the self-hosted compiler; features had to be
implemented twice (C-421); the C++ implementation's never-free memory strategy
stopped working once comptime became central (S-422).

What worked in practice (C-ids):
Replacing the C++ bootstrap with a wasm snapshot + C backend removed the legacy
codebase entirely (~4,000 lines of portable C instead of 80k lines of C++),
enabled bootstrapping any commit from source on any host with a C compiler, and
cut contributor RAM requirements (C-421).

Costs and new problems (C-ids):
A binary blob entered the repo — the author concedes the loss of O(1)
source-only bootstrapping and cites DFSG-style objections (C-421); the bootstrap
wasm must be regenerated when compiler self-build requirements change; -fstage1
users lost the only path to the old async implementation (S-422).

Valid under which conditions:
Works when the project values single-implementation self-hosting and controls its
own IR/backend; the bootstrap-artifact cost matters most for
distro/reproducibility-focused users.

Alternatives that reach the same goal:
Prior-binary bootstrapping, committing generated C, or cleaning generated C —
each enumerated and rejected by the author with stated downsides (S-422).

Causes of limited adoption (if any): n/a (ongoing project).

Counter-evidence and disagreements:
Designer testimony; may understate the ongoing cost of maintaining the wasm
snapshot. The bootstrap path trades reproducibility purity for pragmatism and the
community remains divided (S-422 notes; also C-407's Cranelift-UB nuance shows
"no UB by design" claims need normative-doc verification).

Confidence: high for the facts (primary testimony + release notes);
medium for the interpretation that the wasm-bootstrap trade is net-positive.

Open questions:
What is the maintenance cost of regenerating the bootstrap artifact per release?
How does the C backend affect generated-code quality vs. the LLVM path?

Implications for our design (as hypotheses only):
If Blup ever self-hosts, decide the bootstrap story before the codebase doubles;
a small committed bootstrap artifact is a proven but contested path (C-421, C-425).

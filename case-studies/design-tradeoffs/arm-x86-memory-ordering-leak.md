# ARM vs x86 memory ordering leaking into language semantics

Period and context:
2011–2020. C++11/C11 adopted a single portable memory model; mainstream hardware
was x86-64 (TSO), ARMv7/v8 (weak) and Power (weak); multicore ARM devices
exploded just as the standard shipped.

Problem it was meant to solve (C-ids):
C-508, C-510, C-509: the specified C++11 semantics turned out not correctly
implementable as deployed on Power/ARM when acquire/release and seq_cst combine
(P0668R5, S-512); x86 is TSO so store buffering is observable (C-510); consume
was unusable (C-509). A language hard-coding concurrency semantics to one ISA's
behaviour silently miscompiles or over-constrains on others.

What worked in practice (C-ids):
A collaboration model: compiler/hardware researchers plus Arm and RISC-V
engineers produced rigorous operational/axiomatic models (ARMv8 multicopy-
atomic, C-511; RVWMO, S-501), and WG21 revised the standard model (C++20, C-508).
The RISC-V Atomics ABI pins cross-language instruction mappings (C-513).

Costs and new problems (C-ids):
Repairs are expensive on some ISAs (P0668R5: "expensive to repair" on Power);
consume had to be abandoned after shipping in the standard; seq_cst lowering
costs barriers on weak ISAs; atomics must respect natural alignment (C-512).

Valid under which conditions:
Holds whenever the language exposes shared mutable memory with per-access
ordering guarantees on more than one ISA family; does not apply to
message-passing-only or single-core targets.

Alternatives that reach the same goal:
Exposing only message passing / ownership transfer (C-617 path); restricting
atomics to a small auditable set (C-519 hypothesis).

Counter-evidence and disagreements:
O'Callahan (rr author, 2018) reports tracking his first weak-memory-ordering bug
after a career mostly on x86 (S-522) — even a correct abstract model does not
prevent human ordering bugs; a language also needs a race/ordering detector
(C-618). Kernel + LZ4 documentation show the same spec-vs-hardware gap for
unaligned access (C-515).

Confidence: high on the facts (peer-reviewed + committee + vendor sources);
the "leak" framing is interpretation.

Open questions:
What is the measured performance cost of seq_cst-only atomics on ARM64 in
real workloads? (Not measured this session.)

Implications for our design (as hypotheses only):
Define Kavir's memory model abstractly (happens-before based), expose only
acquire/release/seq_cst (no consume), specify misaligned atomics as a hard
error, and treat per-ISA atomics ABI conformance as a release requirement
(feeds C-519).

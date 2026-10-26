# The AOT pivot: GraalVM Native Image, .NET NativeAOT, and Project Leyden

Period and context:
JIT runtimes dominated 1995–2019; GraalVM Native Image production ~2019; .NET
NativeAOT GA in .NET 8 (2023); Project Leyden JEP 483 delivered JDK 24 (2025),
JEP 544 integrated for JDK 28 (2026).

Problem it was meant to solve (C-ids):
C-205 (startup/warmup cost of JIT); C-204 (JIT cost, per-process code); cloud/
serverless/CLI economics (S-213).

What worked in practice (C-ids):
C-206 (42% startup cut via class-loading/linking cache; PetClinic numbers);
NativeAOT apps have faster startup and smaller footprint and run where JIT is
not allowed (C-210); a real-world port succeeded with reduced footprint (C-223).

Costs and new problems (C-ids):
C-207 (closed world; reflection metadata; fallback images); C-208 (slow builds,
Serial GC default, no JVMTI tools); C-210 (no Reflection.Emit/dynamic loading,
disk size from pre-generated struct-generics, limited diagnostics); C-209
(build-time state baked into binary — hermeticity/reproducibility hazard); C-223
(startup flakiness in practice); JEP 544 refuses an AOT-only mode because
workloads change (C-206).

Valid under which conditions:
Benefits hold for predictable, repetitively-started workloads with representative
training runs (S-205 guidance); closed-world AOT holds for applications that do
not discover code at runtime.

Alternatives that reach the same goal:
Tiered JIT with warmup (status quo, C-224); hybrid AOT cache + JIT (Leyden path,
C-206).

Counter-evidence and disagreements:
C-224 (S-224 argues startup criticism is overrated for long-running servers);
two Oracle-origin groups chose opposite execution models (Native Image pure
closed-world vs HotSpot keeping JIT — conflicting-evidence.md #12).

Confidence: high for documented behavior and measurements; medium for
generalizing magnitude beyond the cited workloads.

Open questions:
Do AOT manifests/metadata scale to plugin ecosystems? What is the maintenance
cost of reachability metadata over time?

Implications for our design (as hypotheses only):
H (C-225): decide the dynamic-vs-AOT-friendly execution contract at v1; if Kavir
has reflection/plugins, design the metadata story at v1 (GraalVM's metadata
repository is evidence of the retrofit's scale, S-211).

# Java generics/type-erasure and the 20-year value-type retrofit

Period and context:
Java 5 generics (2004) implemented by erasure on a fixed JVM; Project Valhalla
started 2014; JEP 401 value objects integrated for JDK 28 preview (2026); JEP 218
(generics over primitives) still Candidate after 12 years.

Problem it was meant to solve (C-ids):
C-217 (boxing cost of primitives in generics, pseudo-specialized APIs); identity
costs — JEP 401: object identity forces allocation and access whether needed or
not (S-207).

What worked in practice (C-ids):
Erasure preserved binary/source compatibility, letting the whole ecosystem
migrate in one compiler release (C-217); the ecosystem lived with erasure for
over two decades without breakage.

Costs and new problems (C-ids):
C-217 (boxing overhead, IntStream proliferation, a dozen open retrofit questions:
arrays, null, overloadings, reflection, inference); C-218 (downstream languages
pay too: Kotlin needed name mangling); JEP 401's non-goals admit null-excluding
layouts will only become possible after future JVM enhancements (S-207) — the
retrofit propagates into GC and object-header design.

Valid under which conditions:
The "erasure was right" conclusion is valid when compatibility with an enormous
installed base dominates; it is not evidence that erasure is the right starting
choice for a new language.

Alternatives that reach the same goal:
C#/.NET reified runtime generics (C-215 sources S-215) — but NativeAOT pays disk
size for pre-generated struct instantiations (C-210). Neither choice is free.

Counter-evidence and disagreements:
C# chose reification; Java chose erasure; both document their own costs (see
conflicting-evidence.md, generic specialization strategy).

Confidence: high for the documented history and mechanics (primary JEPs);
medium for any inference about the counterfactual.

Open questions:
How much boxing cost actually matters on modern JITs? No independent measurement
was fetched this session.

Implications for our design (as hypotheses only):
H (C-226): value types and specialization belong in Kavir's initial type system
and object layout plan, not a later JEP.

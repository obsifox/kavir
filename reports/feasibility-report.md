# Feasibility report — Kavir (working name)

Date: 2026-10-26. Status: APPROVED at the feasibility gate (user approval received
2026-10-26, chat reply "قبوله").
Evidence base: 199 sources, 169 claims (claims.csv), 15 case studies, 20 recorded
conflicts (conflicting-evidence.md). All eight workstreams covered (W1–W8); every
quality dimension has ≥2 claims with primary backing (checker output below).

## 1. Scope and what was researched

Workstreams W1 (systems languages), W2 (managed/VM), W3 (dynamic/productivity),
W4 (compilers/IR/backends), W5 (hardware/ABI), W6 (memory/safety/concurrency),
W7 (ecosystem/tooling), W8 (failures/adoption) — each with framed questions,
fetched primary sources, counter-evidence searches and case studies. Not yet done:
benchmark experiments of our own (Phase 3), trademark database search (user task
before public release), Persian-native review of translations (Phase 4), HOPL
paywalled papers (S-736 lead), region-allocation literature (S-630 lead).

## 2. What is achievable with current technology

Building a new language with an experimental v0.1.0 compiler is demonstrably
achievable: the ecosystem provides reusable backends with permissive licenses
(LLVM Apache-2.0+exceptions, C-402; Cranelift/Wasmtime, C-408; QBE, C-413),
proven IR patterns (SSA + language-level IR: SIL/MIR, C-414, C-415), and a
mature tooling playbook (LSP, C-712; integrated package manager + lockfile,
C-701/C-702; formatter + test runner, C-713, C-714). A one-person/small-team
v0.1.0 targeting a narrow subset on one or two tier-1 targets is realistic;
parity with industrial toolchains on compile speed or peak performance in v0.x
is not a credible goal and is not claimed.

## 3. Ideas already tried and their outcomes

- Reusing LLVM: Rust and Swift layer language IRs over LLVM and document
  workarounds, not regret (C-414, C-415). Zig built on LLVM, then paid a
  two-compiler bootstrap cost and replaced C++ with a wasm snapshot (C-421).
- Custom backend: Go rejected LLVM for compile-speed/ABI control and spent years
  building SSA codegen (C-419, C-420) — viable with institutional funding.
- Light backends: Cranelift claims order-of-magnitude faster codegen with ~14%
  slower code (C-405); rustc's Cranelift backend gave 20–30% faster debug builds
  but lacked unwinding/SIMD (C-417, C-418). QBE: 70%-of-performance goal in 10%
  of the code, 3 targets (C-413).
- Managed runtimes: JVM/.NET hosting is language-independent but JIT warmup and
  startup are real costs (C-204, C-205); AOT retrofits (GraalVM, NativeAOT,
  Leyden) paid closed-world/metadata/diagnostics costs (C-207, C-210, C-206).
- Memory models: ownership (proven, adopted, costly to learn — C-601, C-602,
  C-112); GC (latency engineering, C-613); ARC (cycles manual, C-608); manual
  (dominant CVE source, C-104).
- Gradual typing: unsound-but-free won adoption; soundness not retrofit cheaply
  (C-301, C-302, C-307, C-305).
- Platform-VM strategies: Dart's Chrome-VM plan failed and recovered only via
  Flutter (C-718, C-719, C-721); TypeScript's erasable-layer strategy succeeded
  (C-319).

## 4. Open or costly problems

- No language has made static race prevention cheap (C-617 works; learning cost
  is measurable, C-112; async Rust fragmentation, C-113).
- Compile-time cost of full generics remains a three-way dilemma with no free
  option (C-309, S-312).
- Bit-reproducible builds are more than lockfiles (C-708, C-709).
- ABI freeze is a one-way door with persistent costs (C-220, C-518, C-504).
- The 100x sound-gradual-typing overhead problem (C-305) is unsolved in general.
- AOT + reflection/plugins remain in tension (C-207, C-210).

## 5. Where custom design could add real value

The evidence supports value only in focused niches; a general-purpose language
must beat entrenched ecosystems, which historically requires a killer use case,
sponsor or interop path (C-718–C-727, C-318). Candidate value slots (hypotheses,
to be validated): (a) a small core with a machine-checkable semantics ambition
(C-109 shows it is possible; C-101's open-ended UB is the anti-pattern); (b) an
explicit, auditable unsafe boundary from v1 (H from rust-ownership CS); (c)
concurrency prevention by construction with a backstop detector (C-617, C-618);
(d) integrity-first package tooling (C-730); (e) honest, denominator-stated
safety claims (C-107). None of these is proven demand; the gate decision should
consider whether the user wants to pursue any of them specifically.

## 6. What to reuse instead of rebuild

- Backend/IR: reuse LLVM or Cranelift (or QBE for minimalism) rather than a
  custom optimizer (C-402, C-408, C-413, C-405); keep a thin language IR for
  language-specific optimization (C-414, C-415 pattern).
- Tooling: LSP (C-712), integrated package manager with lockfile + checksums
  (C-701, C-702, C-705), formatter (C-713), edition-style evolution (C-714).
- Bootstrap/CI: existing release infrastructure; WebAssembly as a distribution
  and bootstrap target is a proven pattern (C-410, C-421).
- Memory-safety groundwork: standard sanitizer/fuzzing practices (skill Phase 4).

## 7. Cost, risk, maintenance estimate

Realistic v0.1.0 scope (one tier-1 target, small subset, conformance suite,
docs set): weeks-to-months of focused sessions, not years — provided the scope
ADR stays strict (skill release-0.1.0.md). Ongoing risks: backend version churn
(LLVM releases monthly-cadence ecosystems, C-403/C-409), ABI/registry-policy
lock-in (C-220, C-710), scope creep toward "replace C++" claims that no evidence
supports (charter explicitly refuses such claims without measurement), and
single-maintainer burnout (Perl 6 lesson: 15-year path and identity cost,
C-722–C-724). Name due diligence is clean for "Kavir" on GitHub/npm/crates/PyPI
(name-due-diligence.md); trademark search remains the user's task.

## 8. Experiments needed before architecture decisions

1. Compile-speed/code-quality probe: same small program through LLVM vs
   Cranelift vs QBE with declared methodology (benchmarks/methodology.md) —
   validates the two-tier backend hypothesis (C-425).
2. Ownership-ergonomics probe: borrow-checker friction on 3 small realistic
   programs (validates learning-cost mitigation options, C-112, C-620).
3. Startup/warmup probe if a VM path is considered (C-205, C-224) — likely
   moot if AOT-native is chosen.
4. Lockfile/reproducible-build probe: build twice, compare hashes (C-708, C-709).
5. FFI probe: call a C library through the candidate backend's ABI support
   (C-501–C-506 show per-target work is unavoidable).

## 9. Conflicts in the evidence and unknowns

Twenty recorded conflicts (conflicting-evidence.md). Most consequential for
design: LLVM-or-not (conflict #6 — resolves only against Kavir's goals);
GC-latency claims are single-vendor (conflict #9); checked-exceptions evidence
is normative-vs-testimony with no controlled study (conflict #7); types-vs-bugs
measured effects are modest and mixed (conflict #13); JVM-startup severity
scope-dependent (conflict #11). Unknowns: independent Cranelift-vs-LLVM
benchmarks; region-allocation literature; long-term maintenance cost of AOT
metadata; whether sponsor-less languages can reach durable adoption at all.

## 10. Recommendation

The evidence supports proceeding to Phase 2 (design ADRs) for an experimental,
narrow-scope language with: a small spec with explicit illegal-behaviour
catalogue (not open-ended UB); a reused backend via a thin language IR (backend
ADR after experiment 1); a fixed portable core with an explicit target-specific
layer (C-519/C-520 hypotheses); integrity-first tooling from day one (C-729,
C-730); and a written compatibility promise before any public release (C-731).
It does NOT support: claiming superiority over C/C++/Rust/Go, promising
production readiness at 0.x, or building a custom optimizing backend. If the
user's goal is a production replacement for an existing language, the honest
recommendation is "do not build a new language for that part; reuse existing
infrastructure or contribute to an existing language" — the v0.1.0 scope should
be learning-oriented and honest (README rules in release-0.1.0.md).

---

# Evidence per quality dimension (short sections)

Each section: what the evidence says → what stays unknown → implication for Kavir.

- **performance**: Vendor memory-safety pivots show correctness/speed tension is
  manageable (C-104–C-106); GC pause engineering is vendor-claimed (C-613, C-211,
  C-212); JIT warmup real (C-205). Unknown: independent backend benchmark deltas.
  Implication: pre-declared benchmarks vs C/Rust/Go baselines in Phase 3; no
  superiority claims without them.
- **execution-model**: VM hosting vs native AOT is a documented fork with retrofit
  costs both ways (C-201, C-203, C-207, C-210, C-206); freestanding tier pattern
  established (C-516). Unknown: Kavir workload profile. Implication: execution-model
  ADR at v1 (C-225).
- **compilation**: Backend choice dominates compile time (C-405, C-417, C-419);
  incremental compilation must be designed-in (C-416). Unknown: our workloads.
  Implication: thin language IR + reused backend; compile-time probe experiment.
- **security**: Memory unsafety dominates serious CVEs across vendors with
  different denominators (C-104–C-107, C-606, C-607); supply-chain incidents are
  registry design failures (C-710, C-711). Unknown: Kavir threat model detail.
  Implication: memory safety default; integrity-first tooling (C-730); threat
  model doc in Phase 4.
- **optimization**: Language IR layer enables language-specific optimization before
  backend (C-414, C-415); Cranelift prioritizes top optimizations (C-424). Unknown:
  which optimizations matter for Kavir's niche. Implication: defer; measure later.
- **modules**: JPMS ignores versions (C-221); Go's MVS + import compatibility rule
  is a one-sentence policy (C-703, C-704); lockfile norms exist (C-702, C-706).
  Unknown: none blocking. Implication: version-selection policy ADR at v1 (C-226).
- **portability**: long double/va_list/alignment diverge per target (C-505, C-506,
  C-512); fixed-width core semantics avoid drift (C-101, C-103). Unknown: target
  list. Implication: portable core + explicit target layer (C-519).
- **multi-platform**: Tier definitions exist in mature projects (C-404); cross-target
  testing is a matrix cost (C-208, C-210); wasm is a proven distribution/bootstrap
  target (C-410, C-421). Unknown: Kavir's tier-1 list. Implication: one tier-1 target
  for v0.1.0; second as stretch with CI proof.
- **memory-safety**: Ownership is proven and adopted (C-601, C-602); bugs concentrate
  in unsafe (C-603); ARC cycles are manual (C-608). Unknown: ergonomics cost
  mitigation. Implication: small auditable unsafe boundary; diagnostics investment.
- **concurrency**: Race prevention by construction exists (C-617); channels do not
  prevent bugs (C-619); async costs complexity (C-113, C-615, C-616). Unknown:
  structured-concurrency benefit measurement. Implication: prevention + detector
  backstop; decide async story early.
- **type-system**: Nullability-in-types works (C-215, C-216, C-609); generics
  strategy is a pre-1.0 decision (C-217, C-308); value types retrofit is costly
  (C-218, S-207). Unknown: soundness-contract choice (C-320). Implication: decide
  null-safety + generics + value types before 1.0 (C-226).
- **error-handling**: Exceptions vs results has no controlled study (C-610 vs
  C-611, conflict #7); Go's explicit errors are verbose but programmable (C-612).
  Unknown: ergonomics data. Implication: Phase 2 ADR with an ergonomics experiment.
- **interop-ffi**: C ABI is the substrate (C-114, C-120); per-target ABI work is
  mandatory (C-501–C-506); managed-FFI soundness designs exist (C-222). Unknown:
  none blocking. Implication: FFI layer with per-target conformance pages (C-519).
- **tooling**: Day-one set = build+package manager, formatter, test runner; LSP
  next (C-701, C-713, C-714, C-712, H C-729). Unknown: debugger timing. Implication:
  scope v0.1.0 tooling to the day-one set.
- **diagnostics-dx**: Learning cost gates adoption (C-112); fine-grained error
  locations are a shipped differentiator (C-313); NLL was a major redesign (C-620).
  Unknown: none blocking. Implication: diagnostics quality is a first-class
  constraint from the first error message.
- **stdlib**: Batteries-included carries permanent maintenance cost (C-716); Go
  froze early with compat rules (C-715); Lua's minimal core embeds well (C-316).
  Unknown: Kavir's audience needs. Implication: layered stdlib; freeze policy
  written before public release.
- **reproducibility**: Lockfiles ≠ bit-reproducibility (C-708, C-709, C-702);
  AOT build-time state bakes environment into binaries (C-209). Unknown: our build
  hermeticity. Implication: build-twice check in CI; document what the lockfile
  guarantees (H C-730).
- **testing-quality**: Rust's safe/unsafe bug distribution was found by manual
  study (C-603, C-607); fuzzing/sanitizers are the expected hardening bar (skill
  Phase 4; C-107 methodology lesson). Unknown: none blocking. Implication:
  conformance suite from M1; fuzz targets from M1.
- **evolution-compat**: Compat promises remove adoption fear (C-715); breaking
  resets cost years (C-719, C-722, C-725); editions+tooling let languages move
  (C-714); retrofit stories dominate (C-217, C-218, C-221, C-307). Unknown: none
  blocking. Implication: compatibility policy document before public release.
- **governance-legal**: Identity/branding costs are documented (C-722, C-723);
  licenses are permissive at every reuse point (C-402, C-408); name "Kavir" is
  clean on dev registries (name-due-diligence.md). Unknown: trademark search
  (user task). Implication: license decision is a user checkpoint; trademark
  search before public repo/publicity.

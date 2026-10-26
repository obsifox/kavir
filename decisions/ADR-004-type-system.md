# ADR-004: Core type-system direction

Status: proposed (one-way-door checkpoint — user decision pending)
Date: 2026-10-26
Question: Static or gradual? How is null handled? Sum types? Generics
strategy and timing? Value types now or later?

Options considered (at least two):

  A. Gradual/dynamic-first, types as an optional lint layer later (the
     TypeScript/Python path). Evidence: C-301/C-302 (unsound-but-free wins
     adoption fast). Costs: TS documents soundness as a non-goal; PEP 563
     friction took Python about a decade to unwind (C-307); sound gradual
     typing can cost up to 100x in cast-heavy code (C-305).
  B. Static-first, sound safe subset: types always checked, no null in
     default types, sum types with exhaustive matching, monomorphizing
     generics with explicit bounds, value types from v1. Evidence: C-215/C-216
     (null-in-types works where shipped from the start), C-217/C-218 ( retrofit
     costs are decade-scale: erasure → value objects ~20 years in Java),
     C-308/C-309 (generics strategy must be pre-1.0 — the three-way dilemma),
     C-320 (soundness-contract hypothesis), C-313 (diagnostics quality pays).

Trade-offs: A is friendlier for quick scripts and has the fastest
time-to-first-program, but it mortgages the type system (the exact retrofit
pattern this project's evidence catalogue warns about, C-217/C-218/C-307).
B pays design discipline up front (more checker work before "hello world"
runs) and a steeper first day, in exchange for a type story that never needs
a breaking reset (contrast Dart 2, C-719). For a v0.x experimental language
whose audience opted in deliberately (ADR-001), B's costs are acceptable.

Decision and reasoning (RECOMMENDATION to user — not yet final): Option B,
with these specifics:
- Soundness contract: the safe subset is sound (no unsound holes); any hole
  is `unsafe` and listed in the illegal-behaviour catalogue (C-320; ties to
  ADR-003). We explicitly do NOT promise sound gradual typing.
- Null: absent — nullable T is `Option[T]`; no implicit null anywhere.
- Algebraic data types: enums-as-sums + pattern matching with exhaustiveness
  checking from v1 (cheap for the checker, high leverage for error handling,
  ties to the error-handling ADR).
- Generics: monomorphizing with explicit trait/bound declarations; type
  inference for locals, explicit at API boundaries. Advanced inference and
  higher-kinded features are out of scope until after 1.0 thinking (the
  generic dilemma has no free option, C-309 — monomorphizing matches the
  AOT-native/QBE backend and value-type story).
- Value types: structs are first-class by-value types from v1 (retrofit is
  the documented decade-scale mistake, C-218); no inheritance; composition
  only.
- Numeric types: fixed-width integers only (i8..i64, u8..u64) + f32/f64;
  no implicit narrowing conversions; overflow is checked in safe debug
  builds and defined (wrapping only via explicit operators) otherwise
  (ties to the illegal-behaviour catalogue; C-101/C-103 fixed-semantics
  lesson).

Reversibility (cheap / costly / one-way): one-way — this is the checkpoint
definition of a core type-system direction; retrofits are the single most
documented cost in the evidence base (C-215..C-218, C-307, C-309).

What result would show this was wrong (experiment or metric): (1) Phase 3
conformance suite shows the checker cannot deliver exhaustiveness/borrow/
overflow diagnostics with acceptable effort within the v0.1.0 scope — then
cut generics or ADT scope (record in ADR-005 scope revision), never the
soundness contract; (2) if early users (post-release feedback) demonstrate
that scripts-first ergonomics dominate, add a REPL/lint tier — but the
static-first core stays.

Evidence IDs: C-101, C-103, C-215, C-216, C-217, C-218, C-301, C-302, C-305,
C-307, C-308, C-309, C-313, C-318, C-320, C-719

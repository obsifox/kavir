# Project Charter — Blup (working name)

Date: 2026-10-26
Origin: User request (Persian chat), delegated to the new-language-design skill workflow.

## Goal

Design and build a brand-new programming language, working name **Blup**, and take its
compiler from zero to an honestly-labelled experimental **v0.1.0** published in a GitHub
repository owned by the user.

## Authority and checkpoints

- The user has delegated full authority for research order, file layout, session scoping,
  test design, refactors and milestone planning to the assistant ("autopilot").
- The user stays the owner of: feasibility-gate approval, one-way-door decisions (they may
  reply "decide for me", in which case the ADR records "delegated to Claude" with reasoning),
  final name and license choice, and the publish action.
- GitHub publishing: the user has provided a personal access token for repository
  operations from this environment. Hard rules that override any convenience:
  1. The token is NEVER written into any tracked file, spec, doc, log or script.
  2. The token is never deleted or revoked by the assistant.
  3. No force-push, no destructive operation on existing user resources.
  4. Public release remains gated on v0.1.0 readiness; intermediate work goes to a private
     remote unless the user says otherwise.

## Decisions that must stay open until Phase 1 research ends

Per the skill charter rule, none of the following may be decided before the feasibility
gate (or an explicit user waiver):

1. Target users, purpose and non-goals (including whether "replace C/C++/Java" style
   claims are in scope — default: not claimed without measured evidence).
2. Execution model: AOT native / JIT / interpreter / bytecode VM / WebAssembly target.
3. IR and backend strategy: reuse (LLVM, Cranelift, QBE, WebAssembly) vs. custom backend.
4. Compiler implementation language.
5. Memory and safety model (manual, RC, tracing GC, ownership, hybrid).
6. Type system (static/gradual, generics, inference, sum types, null handling).
7. Error handling model (exceptions, results, panics).
8. Concurrency and parallelism model.
9. Modules, packages, build and versioning/compatibility policy.
10. FFI/ABI strategy.
11. Surface syntax (explicitly the cheapest to change; decided last).

## Language policy

- Every file produced in this project is written in English (research notes, ADRs, spec,
  code comments, README, docs, commit messages).
- Chat with the user is in Persian.
- Persian wiki/help pages appear later as a translation layer only, per
  references/documentation.md.

## Success criteria for v0.1.0

Defined in detail by references/release-0.1.0.md: a small core language subset specified
in spec/, a working compiler or compiler+runtime for at least one tier-1 target, a
conformance test suite, the English documentation set plus in-scope Persian pages,
hardening evidence (fuzzing, sanitizers, dependency audit), CI, and honest README claims
("experimental", explicit "Not yet" list). No superiority claims beyond measured data.

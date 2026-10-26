# Decision log

One line per ADR: number, title, status, date. Superseded decisions stay listed.

## ADRs

- 2026-10-26: ADR-001 purpose/scope/non-goals — ACCEPTED (grounds: user gate
  approval; formalizes the approved recommendation).
- 2026-10-26: ADR-002 implementation strategy (v1: QBE-first native AOT,
  thin IR, compiler in C17) — PROPOSED, then REJECTED by the user.
- 2026-10-26: ADR-002 implementation strategy (v2, rewritten per user
  constraint: custom backend, direct x86-64 machine-code emission, internal
  ELF writer, thin IR, no libc, no cc/C path, Linux x86_64 only,
  second-backend-ready) — ACCEPTED (user decision, same day).
- 2026-10-26: ADR-003 memory & safety model (ownership + explicit unsafe, no
  GC, arenas as library) — PROPOSED (one-way-door checkpoint, user decision
  pending).
- 2026-10-26: ADR-004 core type-system direction (static-first, sound safe
  subset, Option[T], ADTs, monomorphizing generics, value types now) —
  PROPOSED (one-way-door checkpoint, user decision pending).
- 2026-10-26: ADR-005 v0.1.0 scope (narrow honest vertical slice, x86_64-linux
  tier-1, no concurrency/registry/async) — PROPOSED (one-way-door checkpoint,
  user decision pending).

## User decisions (recorded in chat, not ADRs)

- 2026-10-26: feasibility gate APPROVED by the user (chat reply "قبوله") —
  Phase 1 → Phase 2.
- 2026-10-26: working name changed from "Blup" to "Kavir" by the user (name/license are
  user-owned checkpoints); due diligence in name-due-diligence.md.
- 2026-10-26: user REJECTED the QBE-first backend recommendation and imposed
  binding constraints (custom backend, direct machine code, no C path/cc,
  no LLVM/Cranelift/QBE, Linux x86-64 first, thin IR, no libc/direct
  syscalls, second backend addable) — ADR-002 rewritten and accepted on
  these constraints.
- 2026-10-26: ADR-003/004/005 one-line summaries (recommendation, evidence
  IDs, main rejected option) presented; user will answer each SEPARATELY —
  checkpoint 2 remains open for these three.

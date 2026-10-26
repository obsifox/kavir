# Decision log

One line per ADR: number, title, status, date. Superseded decisions stay listed.

## ADRs

- 2026-10-26: ADR-001 purpose/scope/non-goals — ACCEPTED (grounds: user gate
  approval; formalizes the approved recommendation).
- 2026-10-26: ADR-002 implementation strategy (QBE-first native AOT, thin
  language IR, compiler in C17) — PROPOSED (one-way-door checkpoint, user
  decision pending).
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

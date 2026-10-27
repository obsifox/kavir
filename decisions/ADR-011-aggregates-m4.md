# ADR-011: M4 aggregate ABI and representations

- **Status:** ACCEPTED — user decision, 2026-10-27 (structured answers
  to the four-question gate below; each recorded verbatim).
- **Scope:** structs/enums/Option[T] for M4 only (ADR-005 v3 M4 line;
  ADR-004 v0.1.0 subset with the ordered cut list). Nothing else.

## 0. The gate (user answers, 2026-10-27)

1. **Layout** → **A: C layout** — fields in declaration order at
   natural alignment; struct/enum alignment = max field alignment
   (i32-only M4 ⇒ alignment 4, no interior padding); size rounded to
   the alignment. (Rejected: packed; compiler-reordered.)
2. **Passing/returning values > 8 bytes** → **B: all through memory**
   — the user's deliberate choice AGAINST the coordinator's SysV
   two-register recommendation. Values ≤ 8 bytes travel in ONE
   integer register exactly like a SysV INTEGER eightbyte (2-word
   values packed low/high); values 9..16 bytes use memory too (a
   DOCUMENTED deviation from the psABI two-register class); values
   > 16 bytes are SysV-identical memory class.
3. **Tag representation** → **A: uniform i32 tag at word 0**, payload
   words from word 1; enum size = 1 + max payload words. NO niche
   optimisation (dishonest for i32-only values — every i32 is a legal
   Kavir value and M4 has no references to carve niches from).
4. **Option[T] element scope (M4)** → **i32 and aggregates of M4
   types only** (no new scalar widths; Option[Option[T]] allowed —
   monomorphisation synthesises per-T enums named `Option<T>`).

## 1. The M4 surface (temporary grammar; ADR-008 §0)

- `struct Name { f: i32, ... }` — all fields, declaration order in
  literals; `expr.f` chains; `p.f = e;` assignment through `let mut`
  roots; whole-value assignment; copy-on-bind value semantics.
- `enum Name { V, W(i32, i32), ... }` — tag = variant index; ctors
  `Name.V` / `Name.W(e, e)`; bare `Name.V` is a 0-payload ctor.
- `match e { Pat => i32-arm-body, ... , _ => ... }` — exhaustive
  (every variant, or a final `_`); duplicate patterns rejected; `_`
  last only; patterns are flat `Type.Variant(bind...)` /
  `Option.None` / `Option.Some(bind)` / `_` (ADR-004 cut list 1:
  no guards, no nested patterns); **arm bodies are i32 in M4** —
  aggregates are deconstructed into i32 via field paths; binding
  names are arm-local and may not shadow outer variables.
- `Option[T]` builtin: `Option.None` / `Option.Some(e)`; T inferred
  from the expected type at each ctor site (clean diagnostic
  otherwise); Some payload type-checked against T.
- Capacities (clean diagnostics): 24 types, 8 fields/struct,
  16 variants/enum, 8 payload words, 8 struct words (32B), 32 arms,
  3 field steps per assignment, 8 Option nesting levels.

## 2. ABI details (all byte-verified by cases_m4 + run_m4_elf)

- Register classes in DECLARATION order (left-to-right evaluation,
  ADR-010 decision 1 unchanged): ≤2-word args consume the next
  integer slot (RDI RSI RDX RCX R8 R9; R10 = pack scratch, R11D =
  depth); wider args go to the stack in 8-byte units, words packed
  low-address-first within the unit, leftmost stack argument at the
  lowest address.
- Returns: 1 word → EAX; 2 words → EAX packed (low/high); >2 words →
  hidden pointer in RDI (the first real argument shifts to RSI...),
  callee stores result words through RDI into a caller scratch area;
  RAX = the pointer on return (SysV memory-class shape).
- Alignment: every `sub/add rsp, imm` is a multiple of 16 (structural
  check: tests/run_m4_elf.py); outgoing areas
  `((units + 1) & ~1) * 8`.
- Fall-off-end returns zero words (matches the interpreter exactly).
- Deviation ledger vs the psABI: 9..16B aggregates by memory instead
  of two registers. Everything else observed by our subset matches
  SysV. Reversibility: internal convention, no persisted artifact —
  cheap to revisit; the FFI re-decision would need a two-register
  adapter for those sizes only.
- EXPLICIT PASSING RULE (restated for README visibility): an
  aggregate LARGER than 8 bytes ALWAYS travels through memory —
  arguments as a stack copy in 8-byte units, returns via the hidden
  pointer (RDI) — and is NEVER split across a register pair. This is
  the user's locked decision (decision 2, 2026-10-27); changing it
  would be an ADR-level re-decision, not a code cleanup.

## 3. Evidence

- M3's SysV subset (ADR-010) already proved the one-register shape
  for scalars; the eightbyte packing is the same convention applied
  to 2-word values.
- The user's option B halves the backend's aggregate code paths vs
  the coordinator estimate (+~200–300 LOC estimate → measured below).
- Zig/Rust/GC-language history (C-5xx case studies): memory-class
  semantics for wide values is the safe default; the two-register
  optimisation is measurable later from the same IR.

## 4. Implementation notes (bugs the suites caught)

- RDX clobber: the 2-word register pack originally used RDX as
  scratch while RDX itself was argument 2 — caught by
  m4_mix_8args.k's bit-weights; R10 is now the pack scratch.
- Arm-slot allocation: a match arm's body may contain a NESTED match
  whose arms append to the same program-wide pool — the arm's slot
  must be claimed BEFORE its body parses (arm-count fix), else the
  outer match reads the inner's arms.
- Recursive types: the layout pass must run AFTER every annotation
  resolves (forward refs resolve as I32 otherwise) — caught by
  err_recursive_type.k.
- Some-payload type check (Option[i32] vs Some(Point)) added after
  err_option_wrong_elem.k.

# ADR-008: M2 output, arithmetic, if/else — runtime contract and IR extension

- **Status:** Accepted (2026-10-26). Decided by the coordinator under the
  user's delegation for M2 ("small decisions: take yourself and record
  them"); the SCOPE itself is user-mandated (print string + number via the
  write syscall, arithmetic operators, if/else — nothing else).
- **Fills:** the trap-exit-code placeholder left open by ADR-007.
- **Scope:** v0.1.0, i32 only, x86-64 Linux. Wider integers re-open
  ADR-007; new statements re-open this ADR.

## 0. The surface syntax of M2 is TEMPORARY

**Every grammar rule in this ADR is temporary.** Per the user's binding
decision (2026-10-26): the current syntax is provisional and syntax is
finalized LAST, in its own later ADR, after the milestone ladder has
stabilized the semantics. Nothing in the grammar below may be relied on
as a language commitment. The file extension question is handled
separately in ADR-009 and is likewise not final.

## Context

M1 shipped `fn main() -> i32 { return <int>; }` end-to-end. M2 adds the
first observable outputs (write syscall), the first real arithmetic, and
the first control flow. ADR-007 fixed what overflow means (compile-time
for literals, runtime TRAP for `+ - * / %`) but left the trap exit code
and exact message bytes to be pinned "when the runtime lands" — that
happens here. ADR-005 v3 amendment (b) requires every codegen test to
match the interpreter, and ADR-007 decision 2 requires a third,
independently computed expectation.

## Decision 1 — M2 surface (temporary grammar)

```
program := fndef*
fndef   := "fn" IDENT "(" ")" "->" "i32" block
block   := "{" stmt* "}"
stmt    := "return" expr ";"
         | "print" "(" expr ")" ";"          -- print an i32 value
         | "print" "(" STRING ")" ";"        -- print literal bytes
         | "if" "(" expr ")" block
           ("else" block)?                   -- else requires a block
expr    := muldiv (("+"|"-") muldiv)*        -- left associative
muldiv  := unary (("*"|"/"|"%") unary)*      -- left associative
unary   := "-" INT | primary                 -- see the literal rule
primary := INT | "(" expr ")"
```

- `print` adds NOTHING to its output: no trailing newline, no padding,
  no separator. Exact bytes only. Strings may contain `\n` explicitly.
- An `if` condition is an integer expression; nonzero = take the
  then-block, zero = take the else-block when present. Comparison
  operators do NOT exist yet (not in the user-mandated M2 scope); M3
  introduces them.
- `return` stays restricted to the LAST statement of the top-level
  function body (M1 rule, unchanged). A `return` inside an `if` block is
  a compile error in M2 ("functions get real control flow in M3").
- Unary minus applies to integer LITERALS only, and is resolved at
  compile time as part of how a numeral is interpreted (it is not a
  runtime operation in M2):
  - `-2147483648` is THE way to write INT32_MIN. The numeral 2147483648
    is accepted only directly under this unary minus (C-style special
    case); anywhere else it is the usual "does not fit in i32" error.
  - `-N` for any other accepted literal is the negated constant.
  - `--2147483648` and `-(2147483648)` are compile errors (the outer
    minus would need the value +2147483648, which does not fit i32 /
    parens break the literal special case).
- `main` must exist, must be non-empty, and must end with `return`.
  Normal exit code = main's return value & 0xFF (M1 rule, unchanged).
- Other defined functions are parsed and checked but only `main` is
  lowered, compiled, and interpreted (M1 rule, unchanged; calls come in
  M3).

## Decision 2 — Arithmetic semantics (fills ADR-007 for M2)

- All arithmetic is signed 32-bit. Evaluation order: left operand, then
  right operand, then the operation; statements execute in order; `if`
  evaluates the condition, then exactly one branch. This order is
  observable (a print before a trap still prints).
- **M2 does NOT constant-fold arithmetic.** Every `+ - * / %` executes
  at runtime, even when both operands are literals. Rationale: with no
  variables until M3, folding would make the ADR-007 trap path
  unreachable and the mandated trap conformance tests impossible. This
  is an M2-scoped clarification of ADR-007's layering ("constant-folded
  overflow is rejected at compile time" refers to the literal rule, not
  to an optimizer). Folding may be revisited by its own ADR if/when an
  optimizer exists.
- Result out of i32 range → TRAP `integer overflow`.
- Division/remainder truncate toward zero (C / x86 `idiv` semantics):
  7/2=3, -7/2=-3, 7/-2=-3, -7/-2=3; 7%2=1, -7%2=-1, 7%-2=1, -7%-2=-1
  (remainder takes the dividend's sign). The independent oracle
  implements exactly this.
- Division by zero → TRAP `division by zero` (for both `/` and `%`).
- `INT32_MIN / -1` and `INT32_MIN % -1` → TRAP `integer overflow`
  (already fixed by ADR-007's implementation note: both trap without
  flags on x86; M2 keeps the hardware-consistent behaviour for both).

## Decision 3 — Trap contract (pins the ADR-007 placeholder)

A runtime trap writes ONE line to fd 2 (via the write syscall in the
compiled ELF; via stderr in the interpreter) and exits with the FIXED
exit code **101** (pinned here; Rust's panic convention; distinct from
normal exits 0..255 used by return values, from compiler errors 1, and
from usage errors 2). Exact message bytes, shared by the ELF, the
interpreter, and the independent oracle:

- `trap: integer overflow at <file>:<line>:<col>\n`
- `trap: division by zero at <file>:<line>:<col>\n`

`<file>:<line>:<col>` is the position of the OPERATOR token in the
source. `<file>` is the input path exactly as passed to the tool, so a
conformance runner must invoke `build` and `interp` with the identical
path string. The interpreter formats the message at run time; the ELF
embeds one preformatted string per trap site (not deduplicated; limit
1024 sites, clean error beyond; input paths longer than 200 bytes are
rejected at compile time because the message is baked into the binary).
After a trap, no further statements run; stdout already written stays
written (sequential semantics).

## Decision 4 — Output

- `print(expr)`: decimal i32 with `-` for negatives, no padding: 0 →
  `0`, -2147483648 → `-2147483648`. Written via `write(1, ...)`.
- `print("...")`: the literal's bytes, via `write(1, ...)`. Escapes:
  `\n` `\t` `\r` `\\` `\"`; any other escape is a compile error. Bytes
  ≥ 0x80 pass through unchanged (UTF-8 source strings work). Raw
  newlines and EOF inside a string literal are errors. Max 127 bytes
  per literal. write() errors (e.g. closed stdout) are ignored in M2 —
  documented, revisited when error handling lands.

## Decision 5 — IR extension (additive)

The two M1 opcodes keep their encoding and meaning; M2 adds:

| opcode | form | meaning |
|--------|------|---------|
| IR_ADD/SUB/MUL/DIV/REM | `dest, src1, src2` | i32 op; trap per Decision 2 |
| IR_PRINT | `dest` | print vreg as decimal |
| IR_PRINT_STR | `str_idx` | print string-table bytes |
| IR_JMP | `target` | unconditional jump to inst index |
| IR_JMPZ | `cond, target` | jump if vreg == 0 |

- Virtual registers are ssa-style indices; jumps carry instruction
  indices, patched from labels during IR construction (all M2 jumps are
  forward). dump-ir prints `%rN = add %rA, %rB`, `print`, `print_str`,
  `jmp L<n>`, `jmpz %rN, L<n>` and label lines.
- The IR remains an internal, never-persisted format; this extension
  changes nothing about the two M1 opcodes. Flagged to the user per the
  stop-condition rule: judged ADDITIVE, not a one-way-door change
  (nothing persisted, M1 programs assemble identically).

## Decision 6 — x86-64 lowering and the ELF image

- One stack slot per vreg at `[rbp - 8*(vreg+1)]`; prologue
  `push rbp; mov rbp,rsp; sub rsp, <16-aligned slot bytes>`; epilogue
  `leave; ret`. This is the "naive allocation, everything spilled"
  baseline anticipated by ADR-005 v3; a real allocator is not an M2 goal.
- Arithmetic: `mov eax,[lhs]; add/sub eax,[rhs]` / `imul eax,[rhs]`,
  each followed by `jo <trap stub>`. Division/remainder:
  `mov eax,[lhs]; mov ecx,[rhs]; test ecx,ecx; je <div0 stub>; cmp
  ecx,-1; jne ok; cmp eax,0x80000000; je <overflow stub>; ok: cdq;
  idiv ecx` (remainder read from edx for `%`).
- Trap stubs live in a cold pool after main's code; each writes its
  preformatted message to fd 2 then jumps to the shared
  `trap_exit` tail (`exit_group(101)`).
- `kavirrt_print_i32` is emitted INTO the binary by the compiler
  (freestanding runtime, no libc — ADR-002 v2): sign-extend to 64-bit,
  negate safely in 64 bits, repeated `div rcx(10)` digit generation
  into a stack buffer, single `write(1, ...)`.
- The image stays ONE PT_LOAD (R+X): ELF header + program header + code
  + cold stubs + runtime + `_start` + rodata (strings and trap
  messages). Small decisions recorded here:
  - No RW PT_LOAD in M2: there are no globals or mutable data (vregs
    live on the stack), so a writable segment would be speculative; it
    arrives when data actually needs it.
  - Strings live in the executable segment for now (W^X impurity);
    splitting a read-only segment is additive and deferred with the
    data-segment work.

## Decision 7 — Capacity limits (each a clean diagnostic, never a crash)

256 tokens; 32 statements per block; 128 blocks per program; 256
expression nodes per program; 1024 IR instructions per function; 32
string literals × 127 bytes; 1024 trap sites; 256 KiB image buffer;
trap paths ≤ 200 bytes. These are M2 limits, raised additively later.

## Decision 8 — Verification for M2

1. **Independent oracle.** `tests/reference.py` is a third
   implementation of the temporary surface (own lexer/parser/evaluator,
   written from THIS ADR's prose, not from the C sources). It computes
   expected stdout bytes, stderr bytes, and exit code from the
   source-level operands.
2. **Three-way conformance.** For every case:
   `ELF (stdout,stderr,exit) == interpreter == reference`, byte-exact,
   including trap cases (partial stdout before a trap must match).
   `tests/run_m2.py` runs the suite; M1's `run.sh` keeps running
   unchanged as a regression gate.
3. **Fuzzing.** (a) differential grammar-aware mode: generated programs
   (nested arithmetic, boundary numerals, long digit strings, if/else
   nesting) are executed through the compiler and compared against the
   reference evaluator; (b) sanitizer mode: ASan+UBSan build runs
   build/interp/dump-ir on generated + mutated inputs; (c) libFuzzer
   harness extended to the M2 pipeline (interpreter runs quiet, traps
   return instead of exiting). Reports state real numbers and limits;
   the phrase "fuzz clean" is not used (user rule).

## Consequences

- The trap exit code 101 and the two message formats are now binding
  for compiler, interpreter, and oracle; changing them requires a new
  ADR (observable behaviour).
- Division semantics are pinned to truncation toward zero; a floor-
  division alternative would be a language-semantics change (new ADR).
- No-folding means source like `2147483647 + 1` traps at runtime rather
  than erroring at compile time; this is deliberate and tested.
- The syntax being temporary means fixtures may need mechanical updates
  when the finalizing ADR lands; conformance comparisons are against
  behaviour, not spelling.
- print's no-newline rule is observable behaviour; a future stdlib-style
  println can be additive.

## Rejected alternatives

- Compile-time rejection of constant overflow via folding — would make
  the mandated trap tests unreachable in M2 (no runtime values exist
  yet); revisit only with an explicit optimizer ADR.
- Stack-machine IR — less readable dumps than the 3-address vreg IR and
  inconsistent with M1's existing vreg style.
- A template message + runtime number formatting for trap sites — more
  moving parts (parameterized runtime) for no conformance benefit; the
  per-site string is simple and auditable.
- RW data segment in M2 — speculative; nothing needs it yet.

## Evidence references

- ADR-007 (trap semantics, oracle), ADR-005 v3 amendment (b)
  (interpreter as reference), ADR-002 v2 (no cc/libc in the emitted
  program), ADR-006 (all x86-64/Linux constants confined to
  target_x86_64_linux.c).
- User message 2026-10-26: M2 scope + conditions (a)-(h); decisions
  1-5 recorded in the worklog (Task 14).

# ADR-010: Kavir calling convention and frame layout (M3 calls/recursion)

- **Status:** ACCEPTED — user decision, 2026-10-26 (structured answers
  to the ADR-010 gate):
  1. Calling convention: **Option A — System V AMD64 integer subset**
     (the coordinator's recommendation).
  2. Frame layout: **RBP-based frames** (the coordinator's
     recommendation).
  3. Recursion depth limit: **1024** (the coordinator recommended 256;
     the user chose the higher limit — recorded as the user's call).
  4. `let` mutability wording: **`let`/`let mut`** per the ADR-005 v3
     wording (the coordinator recommended a single mutable `let`; the
     user chose the two-keyword pair — recorded as the user's call).
  Implementation followed these decisions in the same session; the M3
  conformance suite (tests/cases_m3, 41 cases) exercises every part of
  this contract three-way (ELF == interpreter == independent oracle).

## 0. Honest reversibility assessment (stated up front)

The convention is INTERNAL: Kavir has no FFI, no separate compilation,
no persisted artifact that encodes it (IR is in-memory only; ELF files
are regenerated). Changing it later invalidates no artifact — it costs
a backend rewrite plus re-verification. That is why the user's gate is
treated as "near" one-way rather than absolute. The parts that ARE
semantically binding (and therefore expensive to change later): the
argument EVALUATION ORDER, the recursion DEPTH LIMIT and its trap
bytes, and the arg-count capacity limit — these are observable through
traps and must match the interpreter and the independent oracle
byte-for-byte (ADR-008 discipline).

## 1. What already exists (evidence from the M2 codebase)

- **ADR-005 v3 (user-approved) already anticipated the answer:** the M3
  ladder line reads "user function calls with arguments (SysV integer
  registers)".
- **The M2 runtime already speaks a SysV subset:** the emitted helper
  `kavirrt_print_i32` receives its argument in EDI (the SysV first
  integer-argument register) and functions return in EAX.
- **16-byte stack alignment is already maintained** at every call site
  in M2 codegen (entry rsp%16==8 after the call push; `push rbp`
  restores 16; slot area rounded to 16).
- **The register budget is tiny by design:** M2 codegen uses only
  EAX/ECX/EDX/ESI/EDI (all caller-saved under SysV) and spills every
  vreg to a stack slot, so adopting SysV register arguments adds no
  register-pressure problem.
- **E5 FFI probe (historical context under ADR-002 v2):** code
  following the platform ABI interoperated with libc/qsort/printf
  without glue. Lesson recorded: standard conventions buy free
  interop; custom conventions buy nothing.
- **ADR-006 roadmap:** T2 = Linux ARM64 (AAPCS64: args in X0-X7, also
  a register convention), then Wasm, Windows x64 (different register
  set: RCX/RDX/R8/R9 + shadow space), macOS ARM64. Per-target ABI
  modules (`abi_<target>`) are already the planned architecture. A
  platform-standard convention per target keeps every future target
  symmetric; a custom convention is an x64-only oddity.

## 2. Options

### Option A — System V AMD64 integer subset (RECOMMENDED)

- First 6 integer arguments in RDI, RSI, RDX, RCX, R8, R9; arguments
  7..n on the caller stack in RIGHT-to-LEFT order (exact SysV stack
  layout); return value in EAX.
- Caller-saved: RAX RCX RDX RSI RDI R8-R11. Callee-saved: RBX RBP
  R12-R15 — M3 Kavir code simply does not allocate to callee-saved
  registers (RBP is the frame pointer), so zero save/restore code is
  needed while the discipline stays SysV-exact.
- 16-byte alignment at every call site (already true in M2).
- Prologue (per function): `push rbp; mov rbp,rsp; sub rsp,<slots>`;
  the 6 register arguments are copied into their frame slots, stack
  arguments are copied from [rbp+16+8i] into frame slots so ALL vreg
  addressing stays uniform [rbp-8*(vreg+1)] (M2 scheme, unchanged).
- Call site: evaluate all argument expressions LEFT-to-RIGHT into
  slots (order pinned, observable via traps); load RDI..R9 (and stack
  args) from slots; `call`; store EAX into the destination slot.
- M3 compile-time capacity: KAVIR_MAX_ARGS = 8 per call (clean
  diagnostic above that) — covers the stack-passing path in tests.
- Evidence FOR: ADR-005 v3 wording; M2 runtime precedent (EDI/EAX);
  the x86-64 psABI is the Linux platform standard (gcc, clang, QBE,
  Cranelift all emit it); gdb understands the frames; exact future
  C-interop shape (if ever re-decided); symmetric with ARM64 T2.
- Evidence AGAINST: ~40-60 LOC more than option B (register loads at
  call sites, stack-arg copying in prologues).
- Cost/risk: LOW. Everything is exercised by M3 tests including >6-arg
  calls.

### Option B — all arguments on the stack (custom, cdecl-flavoured)

- Every argument passed at [rsp+8i] before the call; callee copies to
  frame slots; return in EAX.
- Evidence FOR: the LEAST code (~30 LOC); no register loads; matches
  the "everything spilled" M2 philosophy.
- Evidence AGAINST: contradicts the ADR-005 v3 M3 wording ("SysV
  integer registers") that the user already approved; every future
  target (ARM64, Windows) would need a DIFFERENT mechanism anyway, so
  the "uniformity" is illusory; higher memory traffic (ADR-002 has a
  performance falsification gate); a future C-FFI re-decision would
  need a second convention.

### Option C — custom register convention (own register set)

- Example: first args in EAX/ECX/EDX/ESI, rest on stack.
- Evidence AGAINST: reinvents what the psABI standardizes, with zero
  benefit; breaks the already-SysV M2 runtime helper; worst future
  interop; no alignment with any future target. No credible argument
  FOR it beyond arbitrary register choice. Listed for completeness;
  NOT recommended.

## 3. Frame layout (sub-decision)

### 3a. Frame pointer: KEEP RBP-BASED FRAMES (recommended)

- Layout (current M2 scheme, unchanged): [rbp+8]=return address,
  [rbp+0]=saved RBP, args 7+ at [rbp+16+8i], locals/vregs at
  [rbp-8*(vreg+1)], rsp kept 16-byte aligned at call sites.
- Evidence FOR: recursion (M3's fib) and later aggregates (M4) and
  pointers (M5) benefit from a stable frame reference; gdb backtraces
  walk RBP chains; the scheme is already written, tested, and
  byte-verified in M2.
- Alternative (RSP-only frames) saves one push/pop pair per call and
  frees RBP as a general register — irrelevant while the backend
  spills everything and uses 5 registers. REJECTED for M3; revisit
  only if a register-pressure problem actually materializes.

### 3b. Recursion depth limit (semantics — must be identical in ELF and interpreter)

- ACCEPTED VALUE: limit = **1024 Kavir frames** (user decision,
  2026-10-26), enforced IDENTICALLY in the interpreter (frame-stack
  index) and the generated code (hidden depth value passed in R11D — a
  caller-saved scratch register unused for values in our codegen; each
  prologue does `cmp r11d,1024 / jae <depth-trap stub>` and spills the
  depth; each call site reloads and increments). Frame indices
  0..1023 are legal; entering frame 1024 traps.
- Trap contract (ADR-008 shape): one line to fd 2 —
  `trap: call depth limit exceeded (1024) in fn 'NAME' at FILE:LINE:COL`
  (NAME/position = the function whose prologue rejected the entry,
  position = the 'fn' keyword) — then exit 101. The interpreter and
  the independent oracle produce the identical bytes (verified by
  cases_m3/trap_depth_limit.k and trap_depth_boundary_ok.k).
- Why a limit at all: without one, unbounded recursion ends in
  SIGSEGV inside the ELF but must be caught differently in the
  interpreter — a permanent divergence hole in the three-way oracle.
  With one, deep-recursion behaviour is DEFINED and byte-identical
  everywhere (ADR-008 capacity-limit discipline: "none may be crossed
  by an out-of-bounds access").
- Why the hidden-argument mechanism instead of a memory counter: M2/M3
  have NO RW segment (ADR-008 decision 6 — no globals yet); a global
  counter would force introducing RW data in M3, off-ladder. The
  hidden arg costs ~4 instructions per prologue/call site.

## 4. Decisions taken under the user's delegation (recorded; all reversible, syntax temporary)

These do NOT gate implementation; they follow the standing rule
"small decisions: decide, record, continue". Decision 3 was later
SUPERSEDED by the user's gate answer (let/let mut); kept for history.

1. Argument evaluation order: all argument expressions evaluate
   left-to-right, recursively applying the ADR-008 rule (lhs, rhs,
   op), BEFORE the call executes. Observable via traps between prints;
   identical in interpreter, ELF, and the Python oracle.
2. Comparison operators `< <= > >= == !=` yield i32 1/0; a single
   comparison level, NON-associative (`a < b < c` requires
   parentheses — the C surprise is rejected with a clean diagnostic).
3. SUPERSEDED by the gate answer: the user chose `let` (immutable) /
   `let mut` (mutable) instead of a single mutable `let`. Assignment
   `x = e;` is a statement (not an expression) and only targets
   mutable locals; parameters are immutable.
4. No shadowing within a function (one flat per-function symbol
   table; redeclaration = clean diagnostic).
5. `return` is allowed anywhere a statement is allowed (early return
   inside if/while). This RELAXES the M2 rule ("return only as the
   last top-level statement"); the M2 negative fixture
   err_return_in_if.k became the positive M3 case
   cases_m3/early_return_in_if.k. Falling off the end of any function
   returns 0 (already the documented M2 semantics in BOTH the
   interpreter and the backend).
6. Calls are expressions only in M3 (a bare `f(x);` statement is NOT
   added); calls may appear in return/let/print/if/while expressions.
   Forward references and mutual recursion work (checker collects all
   signatures first).
7. Unary minus stays literal-only (M2 rule, unchanged).
8. IR growth: +8 opcodes (LT LE GT GE EQ NE COPY CALL; arguments
   inlined in CALL). Internal, never persisted, additive — same
   pattern the user accepted for M2.
9. Capacities (clean diagnostics, ADR-008 decision 7 style):
   KAVIR_MAX_ARGS 8 per call, KAVIR_MAX_VARS 64 per function
   (params + lets), KAVIR_MAX_CALL_DEPTH 1024 (runtime, both paths).

## 5. The gate (ANSWERED 2026-10-26 — see the Status block)

The four questions below were put to the user with the evidence of
sections 1-3; the answers are recorded in the Status block and the
implementation conforms to them.

1. Calling convention: A (SysV subset — recommended) / B (all-stack) /
   C (custom registers).
2. Frame layout: RBP frames (recommended) / RSP-only.
3. Recursion depth limit: 256 (recommended) / another value / none.
4. (Recorded here for completeness of the gate) `let` mutability
   wording: mutable-`let` with `=` statements (recommended; no `mut`
   keyword) — or the ADR-005 v3 "let/let mut" pair.

## 6. Amendment 2026-10-26 (M3.1): argument capacity split made explicit

M3.1 review (user instruction: "if ADR-010 says max 6 but the code
accepts 8, reconcile and record") checked the wording against the
code. FINDING: no 6-vs-8 contradiction existed — sections 2 and 4.9
already pinned KAVIR_MAX_ARGS = 8 with stack passing. This amendment
makes the register/stack split explicit so the ADR is self-contained:

- Total arguments per call: **at most 8** (parser-enforced, clean
  diagnostic "too many arguments (limit 8 per call)").
- Split: arguments 1-6 in RDI RSI RDX RCX R8 R9; arguments 7-8 (up to
  2) on the stack: arg 7 at [rsp+0] at the call / [rbp+16] in the
  callee, arg 8 at [rsp+8] / [rbp+24] — right-to-left placement,
  left-to-right EVALUATION (decision 1 unchanged).
- Outgoing area rounding: `((nstack + 1) & ~1) * 8` bytes — an odd
  stack-arg count is padded to 16 so RSP stays 16-byte aligned at
  every call. With the 8-arg cap the largest outgoing area is 16
  bytes (2 stack args need no pad).
- Permanent tests (M3.1): cases_m3/call_args_6.k (register-only,
  weighted), call_args_7.k (1 stack arg + odd-count padding path),
  call_args_8.k (2 stack args), stack_arg_order.k (bit-encoded
  positions incl. a nested 8-arg call inside a stack argument), and
  tests/run_m31.py (ELF structural scan: every sub/add rsp immediate
  is a multiple of 16; the expected `sub rsp,16` sites are present).

## Evidence references

- x86-64 psABI (System V AMD64 ABI): integer args RDI RSI RDX RCX R8
  R9, stack args right-to-left, EAX return, 16-byte alignment —
  the Linux platform standard; already partially in use by the M2
  runtime (EDI argument, EAX return).
- ADR-005 v3 M3 line: "user function calls with arguments (SysV
  integer registers)" — user-approved anticipation of option A.
- ADR-008 decisions 2/3/6/7: evaluation-order pinning, trap contract,
  no RW segment yet, capacity-limit discipline.
- ADR-006: per-target ABI module architecture; ARM64/Windows differ
  from SysV — per-target standard conventions stay symmetric.
- benchmarks/e5-ffi-probe.md: platform-ABI code interoperated with
  libc/qsort/printf without glue (historical context; ADR-002 v2 keeps
  C FFI out of the v0.1.0 path).
- M2 codebase facts: kavirrt_print_i32(edi); EAX return; 16-byte
  alignment at call sites; all-vregs-spilled allocation.

# Kavir compiler (M1 + M1.1 stabilization)

The Kavir compiler is written in C17 and builds with gcc (bootstrap only —
see decisions/ADR-002-implementation-strategy.md: the programs it emits
NEVER pass through cc/as/ld). It emits static x86_64-linux ELF executables
with no libc dependency, using direct machine-code generation and an
internal ELF writer.

## Build

    make            # builds ./kavir (C17, -Wall -Wextra -Werror)
    make kavir-asan # ASan+UBSan build for fuzzing
    make test       # conformance suite (16 checks)
    make fuzz       # quick sanitizer fuzz (2000 inputs / 60 s)
    make fuzz-long FUZZ_SECS=600   # longer campaign

C LOC accounting (definition: src/*.c + src/kavir.h; shell/Python
tooling reported separately — see reports/milestones/M1.md):

    sh scripts/loc.sh

## Try it

    ./kavir build tests/fixtures/m1_42.k -o /tmp/answer
    /tmp/answer
    echo $?         # -> 42

Flags and input can come in any order (`build -o out in.k` also works).
Other commands:

    ./kavir interp tests/fixtures/m1_42.k   # run the IR interpreter
                                            # (reference semantics); exits 42
    ./kavir dump-ir tests/fixtures/m1_42.k  # print the IR

## Fuzzing (ADR-005 v3 amendment b, upgraded M1.1)

Two complementary tools; neither is a substitute for the other:

1. **Driver** (`make fuzz`, gcc-friendly): scripts/fuzz.py feeds
   grammar-aware generated programs (long digit strings, i32/u64/u64+1
   boundary numerals, identifiers straddling the 63-char limit) and byte
   mutations through `build`, `interp` AND `dump-ir` of the ASan+UBSan
   build. Honest limits: NOT coverage-guided; the driver CLI's
   arg-parsing and file-IO error paths are not fuzzed; no persistent
   mode; no cross-run corpus.
2. **libFuzzer harness** (`make fuzz-libfuzzer`, REQUIRES clang — gcc
   does not ship libFuzzer): fuzz/fuzz_frontend.c runs one input through
   lex -> parse -> check -> ir_build -> interp -> emit in-process with
   real coverage feedback. This environment has no clang, so the harness
   ships in-repo and runs wherever clang exists (CI later).

Only describe a fuzz run as "clean" together with its input count,
duration, and the limits above.

## Integer overflow (ADR-007)

Literals that do not fit i32 are compile-time errors (distinct messages
for >i32 and >u64). From M2, runtime arithmetic overflow TRAPS — no
silent wrap. The lexer/checker path contains no signed-overflow UB by
construction (unsigned accumulation, check-before-multiply).

## Scope (M1, per decisions/ADR-005-v0.1.0-scope.md v3)

Supported surface: `fn name() -> i32 { return <int-literal>; }`.
Everything else reports a file:line:col diagnostic naming the milestone
that will introduce it. Milestone ladder and the M3 calibration
checkpoint are defined in the ADR.

## Layout

    src/lexer.c                   tokens + spans (unsigned numeral path)
    src/parser.c                  M1 grammar
    src/check.c                   M1 semantic rules (unsigned range check)
    src/ir.c                      IR build + text dump
    src/interp.c                  IR interpreter (reference semantics)
    src/target_x86_64_linux.c     ALL x86-64/Linux specifics (ADR-006 rule)
    src/main.c                    driver (build / interp / dump-ir)
    tests/                        conformance suite + fixtures (16 checks)
    fuzz/fuzz_frontend.c          libFuzzer harness (clang-only)
    scripts/fuzz.py               sanitizer fuzz driver (gcc-friendly)
    scripts/loc.sh                LOC accounting with explicit definition

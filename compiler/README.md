# Kavir compiler (M1)

The Kavir compiler is written in C17 and builds with gcc (bootstrap only —
see decisions/ADR-002-implementation-strategy.md: the programs it emits
NEVER pass through cc/as/ld). It emits static x86_64-linux ELF executables
with no libc dependency, using direct machine-code generation and an
internal ELF writer.

## Build

    make            # builds ./kavir (C17, -Wall -Wextra -Werror)
    make kavir-asan # ASan+UBSan build for fuzzing
    make test       # conformance suite (ELF vs interpreter cross-check)
    make fuzz       # 300-iteration frontend fuzzer under sanitizers

## Try it

    ./kavir build tests/fixtures/m1_42.k -o /tmp/answer
    /tmp/answer
    echo $?         # -> 42

Other commands:

    ./kavir interp tests/fixtures/m1_42.k   # run the IR interpreter
                                            # (reference semantics); exits 42
    ./kavir dump-ir tests/fixtures/m1_42.k  # print the IR

## Scope (M1, per decisions/ADR-005-v0.1.0-scope.md v3)

Supported surface: `fn name() -> i32 { return <int-literal>; }`.
Everything else reports a file:line:col diagnostic naming the milestone
that will introduce it. Milestone ladder and the M3 calibration
checkpoint are defined in the ADR.

## Layout

    src/lexer.c                   tokens + spans
    src/parser.c                  M1 grammar
    src/check.c                   M1 semantic rules
    src/ir.c                      IR build + text dump
    src/interp.c                  IR interpreter (reference semantics)
    src/target_x86_64_linux.c     ALL x86-64/Linux specifics (ADR-006 rule)
    src/main.c                    driver (build / interp / dump-ir)
    tests/                        conformance suite + fixtures
    scripts/fuzz.py               sanitizer fuzz driver

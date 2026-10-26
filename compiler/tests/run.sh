#!/bin/sh
# Kavir M1 conformance tests (ADR-005 v3: walking skeleton + amendment b).
# Rule: EVERY codegen test compares the ELF behaviour against the IR
# interpreter on the same program. A mismatch is a conformance failure.
# NOTE: no `set -e` — the programs under test legitimately exit nonzero,
# and every failure path below handles errors explicitly.
cd "$(dirname "$0")/.."

make -s kavir >/dev/null || { echo "FAIL build"; exit 1; }

pass=0

check_program() {
    fixture="$1"
    expect="$2"
    name="$3"

    ref=$(./kavir interp "$fixture" 2>/dev/null; echo $?)
    ./kavir build "$fixture" -o /tmp/kavir_test_prog >/dev/null 2>&1 \
        || { printf 'FAIL %s: build failed\n' "$name"; exit 1; }
    got=$( /tmp/kavir_test_prog; echo $? )

    if [ "$got" = "$expect" ] && [ "$ref" = "$expect" ] && [ "$got" = "$ref" ]; then
        pass=$((pass+1))
        printf 'PASS %-24s elf=%s interp=%s (want %s)\n' "$name" "$got" "$ref" "$expect"
    else
        printf 'FAIL %-24s elf=%s interp=%s (want %s)\n' "$name" "$got" "$ref" "$expect"
        exit 1
    fi
}

expect_error() {
    fixture="$1"
    name="$2"
    if ./kavir build "$fixture" -o /tmp/kavir_test_err 2>/tmp/kavir_test_errlog; then
        printf 'FAIL %-24s expected compile error, got success\n' "$name"
        exit 1
    fi
    if grep -q 'error' /tmp/kavir_test_errlog && grep -qE ':[0-9]+:[0-9]+:' /tmp/kavir_test_errlog; then
        printf 'PASS %-24s rejected with span diagnostic\n' "$name"
    else
        printf 'FAIL %-24s error without file:line:col span:\n' "$name"
        cat /tmp/kavir_test_errlog
        exit 1
    fi
}

check_program tests/fixtures/m1_42.k 42 "m1_42"

expect_error tests/fixtures/err_no_main.k  "no_main"
expect_error tests/fixtures/err_type.k     "literal_overflow_i32"
expect_error tests/fixtures/err_syntax.k   "missing_semicolon"

if ./kavir dump-ir tests/fixtures/m1_42.k | grep -q 'ret %r0'; then
    printf 'PASS %-24s IR text dump\n' "dump_ir"
else
    printf 'FAIL %-24s IR dump missing ret\n' "dump_ir"
    exit 1
fi

printf 'M1 conformance: %d checks passed\n' "$pass"
exit 0

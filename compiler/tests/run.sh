#!/bin/sh
# Kavir M1.1 conformance suite (ADR-005 v3 amendment b + ADR-007).
# Rule 1 (amendment b): EVERY codegen test compares the ELF behaviour
#   against the IR interpreter on the same program; a mismatch fails.
# Rule 2 (ADR-007 oracle rule): expected values are INDEPENDENT of the
#   compiler -- hand-written constants in this file (never derived from
#   kavir's own output). M2 adds a third, separately-computed source
#   (tests/reference.py) for arithmetic cases.
# NOTE: no `set -e` -- programs under test legitimately exit nonzero;
# every failure path below handles errors explicitly.
cd "$(dirname "$0")/.."

make -s kavir >/dev/null || { echo "FAIL build"; exit 1; }

pass=0

ok()  { pass=$((pass+1)); printf 'PASS %s\n' "$1"; }
die() { printf 'FAIL %s\n' "$1"; printf 'M1.1 conformance: %d checks passed before failure\n' "$pass"; exit 1; }

# check_program <fixture> <expected-exit> <name>
# The expected value below is an INDEPENDENT constant, and BOTH the ELF
# program and the interpreter must produce exactly it.
# M4.1 follow-up: every exec is wrapped in coreutils timeout(1); exit
# code 124 means the timeout fired and is reported with a clear message
# instead of hanging the suite.
check_program() {
    fixture="$1"; expect="$2"; name="$3"

    ref=$(timeout 60 ./kavir interp "$fixture" 2>/dev/null; echo $?)
    if [ "$ref" = "124" ]; then die "$name: interpreter timed out (60s)"; fi
    ./kavir build "$fixture" -o /tmp/kavir_test_prog >/dev/null 2>&1 \
        || die "$name: build failed"
    got=$( timeout 60 /tmp/kavir_test_prog; echo $? )
    if [ "$got" = "124" ]; then die "$name: ELF run timed out (60s)"; fi

    if [ "$got" = "$expect" ] && [ "$ref" = "$expect" ] && [ "$got" = "$ref" ]; then
        ok "$name (elf=$got interp=$ref want=$expect)"
    else
        die "$name elf=$got interp=$ref want=$expect"
    fi
}

# expect_error <fixture> <name> <regex>
# The diagnostic must (a) exist, (b) carry a file:line:col span, and
# (c) match the specific pattern for THIS case -- generic "some error"
# acceptance is not enough (M1.1 review item).
expect_error() {
    fixture="$1"; name="$2"; pattern="$3"
    timeout 120 ./kavir build "$fixture" -o /tmp/kavir_test_err 2>/tmp/kavir_test_errlog
    rc=$?
    if [ "$rc" = "124" ]; then die "$name: build timed out (120s)"; fi
    if [ "$rc" = "0" ]; then
        die "$name: expected compile error, got success"
    fi
    if grep -qE ':[0-9]+:[0-9]+:' /tmp/kavir_test_errlog \
           && grep -qE "$pattern" /tmp/kavir_test_errlog; then
        ok "$name (rejected: $(grep -oE "$pattern" /tmp/kavir_test_errlog | head -1))"
    else
        printf 'FAIL %s: missing span or missing pattern "%s"; got:\n' "$name" "$pattern"
        cat /tmp/kavir_test_errlog
        die "$name"
    fi
}

# ---- positive: whole pipeline, independent expected values ----
check_program tests/fixtures/m1_42.k      42  "m1_42"
check_program tests/fixtures/lit_i32_max.k 255 "lit_i32_max (INT32_MAX; 2147483647&0xFF)"

# ---- negative: literal boundaries (ADR-007, M1.1 review items 1+2) ----
expect_error tests/fixtures/err_lit_i32_ovf.k  "lit_2147483648"        'does not fit in i32'
expect_error tests/fixtures/err_lit_u32_ovf.k  "lit_4294967296"        'does not fit in i32'
expect_error tests/fixtures/err_lit_i64_max.k  "lit_i64_max"           'does not fit in i32'
expect_error tests/fixtures/err_lit_2p63.k     "lit_2^63"              'does not fit in i32'
expect_error tests/fixtures/err_lit_2p63p1.k   "lit_2^63_plus_1"       'does not fit in i32'
expect_error tests/fixtures/err_lit_2p64.k     "lit_2^64"              'integer literal too large'
expect_error tests/fixtures/err_lit_1e20.k     "lit_10^20"             'integer literal too large'

# ---- negative: identifier length limit (M1.1 review item 5) ----
expect_error tests/fixtures/err_ident_len.k    "ident_over_63_chars"   'exceeds the maximum length of 63'

# ---- negative: pre-existing M1 cases, now pattern-checked ----
expect_error tests/fixtures/err_no_main.k      "no_main"               "no 'main' function"
expect_error tests/fixtures/err_type.k         "literal_overflow_i32"  'does not fit in i32'
expect_error tests/fixtures/err_syntax.k       "missing_semicolon"     "expected ';'"

# ---- CLI arg handling (M1.1 review item 5) ----
if ./kavir build -o /tmp/kavir_argprobe tests/fixtures/m1_42.k >/dev/null 2>&1 \
   && [ "$(/tmp/kavir_argprobe; echo $?)" = "42" ]; then
    ok "cli_flag_order (build -o out in.k)"
else
    die "cli_flag_order: 'build -o out in.k' must work"
fi
if ./kavir build tests/fixtures/m1_42.k 2>/tmp/kavir_arglog; then
    die "cli_missing_o: build without -o must fail"
else
    grep -q "no output path" /tmp/kavir_arglog \
        && ok "cli_missing_o (clear message)" \
        || die "cli_missing_o: message should mention the missing output path"
fi

# ---- IR dump smoke ----
if ./kavir dump-ir tests/fixtures/m1_42.k | grep -q 'ret %r0'; then
    ok "dump_ir"
else
    die "dump_ir: IR dump missing ret"
fi

printf 'M1.1 conformance: %d/%d checks passed\n' "$pass" "$pass"
exit 0

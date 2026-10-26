#!/usr/bin/env python3
"""tests/run_m31.py — M3.1 ELF structural checks (ADR-010 alignment).

The three-way suite (run_m2/run_m3) pins BEHAVIOUR byte for byte; this
script pins the ELF-side STRUCTURE of ADR-010's stack rule, which
behaviour alone cannot observe (a misaligned RSP would not crash this
backend - no aligned SSE instructions are emitted):

  1. Every `sub rsp, imm` / `add rsp, imm` in the emitted image is a
     multiple of 16. The backend emits only these encodings:
       48 83 EC ib    sub rsp, imm8     (prologues, call sites)
       48 81 EC id    sub rsp, imm32
       48 83 C4 ib    add rsp, imm8     (outgoing-area cleanup)
       48 81 EC id    add rsp, imm32
     Invariant chain: the kernel starts _start with rsp % 16 == 0;
     `call` pushes 8; the callee's `push rbp` restores 16; frame
     allocation and outgoing argument areas must therefore keep rsp
     16-aligned at every call site (ADR-010 section 2, option A).

  2. The stack-argument rounding is visible: a 7-argument call (1
     stack argument) must contain a `sub rsp, 16` call site (1 arg
     padded to 16 bytes); an 8-argument call (2 stack arguments) also
     allocates exactly 16 (2 args need no pad). These are the
     ((nstack + 1) & ~1) * 8 sites from ADR-010. (A `sub rsp, 32`
     site would need 4 stack arguments - beyond KAVIR_MAX_ARGS=8 -
     so 32-immediates here come from prologue frames, not call sites.)

  3. The built programs actually run and exit 0 (smoke).

The interpreter has no physical stack: its side of the arg contract
(positional mapping, arg COUNT, evaluation order) is pinned three-way
by the call_args_6/7/8 and stack_arg_order fixtures in run_m3.py.

Run: python3 tests/run_m31.py   (after `make`)
"""
import os
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, ".."))

CASES = [
    # sub16: minimum count of `sub rsp,16` sites (stack-arg call sites
    # NOTE: with KAVIR_MAX_ARGS=8 the largest outgoing area is 16 bytes
    # and/or padded prologues); None = do not check
    ("call_args_6.k",        {"sub16": None}),
    ("call_args_7.k",        {"sub16": 1}),    # 6 w7 calls? no: 1+ pad sites
    ("call_args_8.k",        {"sub16": 1}),    # 2 stack args = 16 bytes
    ("stack_arg_order.k",    {"sub16": 6}),    # 7 calls with 2 stack args
    ("labels_50_fns_ok.k",   {"sub16": None}),
]


def scan(buf):
    """Return (all_imms, sub16_count) for rsp add/sub."""
    imms = []
    sub16 = 0
    n = len(buf)
    for i in range(n - 7):
        b = buf[i:i + 3]
        if b == b"\x48\x83\xEC" or b == b"\x48\x83\xC4":
            imms.append(buf[i + 3])                       # signed imm8
        elif b == b"\x48\x81\xEC" or b == b"\x48\x81\xC4":
            imms.append(struct.unpack("<i", buf[i + 3:i + 7])[0])
    for i in range(n - 4):
        if buf[i:i + 4] == b"\x48\x83\xEC\x10":
            sub16 += 1
    return imms, sub16


def main():
    kavir = os.path.join(ROOT, "kavir")
    if not os.path.exists(kavir):
        sys.exit("run_m31: ./kavir missing - run 'make' first")

    passed = failed = 0
    for case, expect in CASES:
        path = os.path.join(HERE, "cases_m3", case)
        with tempfile.NamedTemporaryFile(delete=False) as tf:
            out_path = tf.name
        try:
            os.unlink(out_path)
        except OSError:
            pass
        b = subprocess.run([kavir, "build", path, "-o", out_path],
                           capture_output=True, timeout=60)
        if b.returncode != 0:
            print("FAIL %s (build: %s)" % (case, b.stderr.decode()[:200]))
            failed += 1
            continue
        buf = open(out_path, "rb").read()

        imms, sub16 = scan(buf)
        bad = [v for v in imms if v % 16 != 0]
        ok = not bad
        msg = "%d rsp add/sub sites, imms%%16: %s, sub16=%d" % (
            len(imms), sorted(set(imms)), sub16)
        if expect["sub16"] is not None and sub16 < expect["sub16"]:
            ok = False
            msg += "; expected >=%d `sub rsp,16` sites, got %d" % (
                expect["sub16"], sub16)

        # smoke: the binary must run and exit 0 (all five cases exit 0)
        r = subprocess.run([out_path], capture_output=True, timeout=60)
        if r.returncode != 0:
            ok = False
            msg += "; run exited %d" % r.returncode

        if ok:
            print("PASS %s (%s)" % (case, msg))
            passed += 1
        else:
            print("FAIL %s (%s; misaligned: %s)" % (case, msg, bad[:10]))
            failed += 1
        try:
            os.unlink(out_path)
        except OSError:
            pass

    print("M3.1 ELF structural checks: %d/%d passed" % (passed, passed + failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())

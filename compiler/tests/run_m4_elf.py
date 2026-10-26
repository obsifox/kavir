#!/usr/bin/env python3
"""tests/run_m4_elf.py — M4 ELF structural checks (ADR-011 alignment).

run_m2/run_m3/run_m4 pin BEHAVIOUR byte for byte; this script pins the
ELF-side STRUCTURE of ADR-011's stack discipline, which behaviour alone
cannot observe (a misaligned RSP would not crash this backend — no
aligned SSE instructions are emitted):

  1. Every `sub rsp, imm` / `add rsp, imm` in the emitted image is a
     multiple of 16. The backend emits only:
       48 83 EC ib    sub rsp, imm8     (prologues, call sites)
       48 81 EC id    sub rsp, imm32
       48 83 C4 ib    add rsp, imm8     (outgoing-area cleanup)
       48 81 C4 id    add rsp, imm32
     Invariant chain: the kernel starts _start with rsp % 16 == 0;
     `call` pushes 8; the callee's `push rbp` restores 16; frame
     allocation and outgoing aggregate areas must therefore keep rsp
     16-aligned at every call site (ADR-010 s2 A; ADR-011 s3).

  2. M4 aggregate call sites are visible: an argument wider than 8
     bytes contributes ((words + 1) & ~1) * 8 stack bytes, so fixtures
     with 3- and 5-word arguments must contain `sub rsp, imm` sites at
     16/32/48... immediates (the m4_args_stack fixture guarantees
     >=3 distinct stack-unit sites). Spot-checked via the sub-16 count.

  3. The built programs run and exit as their three-way cases do.

Run: python3 tests/run_m4_elf.py   (after `make`)
"""
import os
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, ".."))

CASES = [
    # (fixture, expected-runs-clean, min `sub rsp,16` sites)
    ("m4_args_register.k", 0, None),
    ("m4_args_stack.k",    0, 2),     # 3-word args -> 16-byte areas
    ("m4_args_hidden.k",   0, 1),
    ("m4_mix_8args.k",     0, None),  # 3 units -> sub rsp,32 (not 16)
    ("m4_struct_nested.k", 0, None),
    ("m4_option_abi.k",    0, None),
]


def scan(buf):
    imms = []
    sub16 = 0
    n = len(buf)
    for i in range(n - 7):
        b = buf[i:i + 3]
        if b == b"\x48\x83\xEC" or b == b"\x48\x83\xC4":
            imms.append(buf[i + 3])                      # signed imm8
        elif b == b"\x48\x81\xEC" or b == b"\x48\x81\xC4":
            imms.append(struct.unpack("<i", buf[i + 3:i + 7])[0])
    for i in range(n - 4):
        if buf[i:i + 4] == b"\x48\x83\xEC\x10":
            sub16 += 1
    return imms, sub16


def main():
    kavir = os.path.join(ROOT, "kavir")
    if not os.path.exists(kavir):
        sys.exit("run_m4_elf: ./kavir missing — run 'make' first")

    passed = failed = 0
    for case, want_rc, min_sub16 in CASES:
        path = os.path.join(HERE, "cases_m4", case)
        with tempfile.NamedTemporaryFile(delete=False) as tf:
            out_path = tf.name
        try:
            os.unlink(out_path)
        except OSError:
            pass
        b = subprocess.run([kavir, "build", path, "-o", out_path],
                           capture_output=True, timeout=120)
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
        if min_sub16 is not None and sub16 < min_sub16:
            ok = False
            msg += "; expected >=%d `sub rsp,16` sites, got %d" % (
                min_sub16, sub16)

        r = subprocess.run([out_path], capture_output=True, timeout=120)
        if r.returncode != want_rc:
            ok = False
            msg += "; run exited %d (want %d)" % (r.returncode, want_rc)

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

    print("M4 ELF structural checks: %d/%d passed"
          % (passed, passed + failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())

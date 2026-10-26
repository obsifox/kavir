#!/usr/bin/env python3
"""Kavir M1 fuzz driver (ADR-005 v3 amendment b).

Feeds random mutations of the seed fixture to the ASan+UBSan compiler
build through both the `build` and `interp` paths (lexer, parser,
checker, IR, codegen). Any sanitizer report, crash (SIGABRT/SIGSEGV) or
hang is a failure and the input is preserved as fuzz-crash-*.bin.
Clean compile errors (exit 1) are the expected outcome for garbage.
"""
import os
import random
import subprocess
import sys
import tempfile

SEED = "tests/fixtures/m1_42.k"
N = int(sys.argv[1]) if len(sys.argv) > 1 else 300

random.seed(20261026)
seed = open(SEED, "rb").read()

bad = 0
for i in range(N):
    data = bytearray(seed)
    for _ in range(random.randint(1, 8)):
        op = random.randint(0, 2)
        if op == 0 and data:
            data[random.randrange(len(data))] = random.randrange(256)
        elif op == 1:
            data.insert(random.randrange(len(data) + 1), random.randrange(256))
        elif data:
            del data[random.randrange(len(data))]

    fd, path = tempfile.mkstemp(suffix=".k")
    os.write(fd, bytes(data))
    os.close(fd)

    env = dict(os.environ)
    env["ASAN_OPTIONS"] = "detect_leaks=0"
    try:
        for mode in (["build", path, "-o", "/tmp/kavir_fuzz_out"],
                     ["interp", path]):
            try:
                r = subprocess.run(["./kavir-asan"] + mode,
                                   capture_output=True, timeout=10, env=env)
                err = r.stderr.decode("utf-8", "replace")
                crashed = (r.returncode in (134, 139, -6, -11)
                           or "AddressSanitizer" in err
                           or "runtime error" in err)
                if crashed:
                    bad += 1
                    keep = f"fuzz-crash-{i}.bin"
                    open(keep, "wb").write(bytes(data))
                    print(f"CRASH iter={i} mode={mode[0]} rc={r.returncode} "
                          f"saved={keep}")
                    print(err[:2000])
                    break
            except subprocess.TimeoutExpired:
                bad += 1
                print(f"TIMEOUT iter={i} mode={mode[0]}")
                break
    finally:
        os.unlink(path)

print(f"fuzz done: {N} iterations, {bad} problems")
sys.exit(1 if bad else 0)

#!/usr/bin/env python3
"""Kavir frontend+codegen fuzzer (ADR-005 v3 amendment b; upgraded M1.1).

Input classes
  1. grammar-aware generated programs: valid-shaped M1 surface seeded with
     adversarial content — LONG digit strings (10..40 digits), boundary
     constants (2^31-1..2^64+2, 10^20), identifiers straddling the 63-char
     limit (1/63/64/70/200 chars), random comments/whitespace;
  2. byte mutations of the fixture corpus (kept from M1).

Every input runs through ./kavir-asan (ASan+UBSan, gcc build) via `build`,
`interp` AND `dump-ir`. A sanitizer report, crash (SIGABRT/SIGSEGV), or
hang is a failure and the input is preserved as fuzz-crash-*.txt.
Clean compile errors (exit 1) are the expected outcome for garbage.

Known limits — reported honestly (M1.1 review item 3):
  - NOT coverage-guided. The libFuzzer harness (fuzz/fuzz_frontend.c)
    provides that, but libFuzzer is clang-only and this environment has
    gcc only; `make fuzz-libfuzzer` runs it wherever clang exists.
  - The `kavir` CLI arg-parsing paths and file-IO error paths are NOT
    fuzzed: only the three subcommands with a real temporary file arg.
  - No persistent-process mode and no corpus accumulation between runs.
"""
import argparse
import os
import random
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, ".."))
FIXTURES = os.path.join(ROOT, "tests", "fixtures")

BOUNDARY_NUMERALS = [
    "0", "1", "7", "9", "10", "99", "100",
    "2147483646", "2147483647", "2147483648", "2147483649",
    "4294967295", "4294967296", "4294967297",
    "9223372036854775806", "9223372036854775807",
    "9223372036854775808", "9223372036854775809",
    "18446744073709551614", "18446744073709551615",
    "18446744073709551616", "18446744073709551617",
    "99999999999999999999", "100000000000000000000",
]

IDENT_LEN_CHOICES = [1, 2, 8, 62, 63, 64, 65, 70, 200]


def gen_ident(rng):
    n = rng.choice(IDENT_LEN_CHOICES)
    # first char alpha/underscore, rest alnum/underscore
    first = rng.choice("_abcdefghijklmnopqrstuvwxyzABC")
    rest = "".join(rng.choice("_abcdefghijklmnopqrstuvwxyzABCXYZ0123456789")
                   for _ in range(max(0, n - 1)))
    return first + rest


def gen_numeral(rng):
    r = rng.random()
    if r < 0.45:                       # LONG digit strings
        length = rng.randint(10, 40)
        first = rng.choice("123456789")
        return first + "".join(rng.choice("0123456789")
                               for _ in range(length - 1))
    if r < 0.85:                       # exact boundary constants
        return rng.choice(BOUNDARY_NUMERALS)
    return str(rng.randint(0, 2 ** 32))


def gen_grammar_aware(rng):
    """Mostly-shaped M1 programs with adversarial leaves."""
    lines = []
    if rng.random() < 0.7:
        lines.append("// generated fuzz program")
    name = gen_ident(rng)
    if name and (name[0].isdigit() or name in ("fn", "return", "i32")):
        name = "_" + name
    body_name = gen_ident(rng)
    if body_name and (body_name[0].isdigit() or body_name in ("fn", "return", "i32")):
        body_name = "_" + body_name
    lines.append("fn %s() -> i32 {" % name)
    for _ in range(rng.randint(1, 2)):
        lines.append("    %s" % ("// noise " + gen_ident(rng)
                                 if rng.random() < 0.2 else ""))
    val = gen_numeral(rng)
    semi = ";" if rng.random() < 0.9 else ""
    lines.append("    return %s%s" % (val, semi))
    lines.append("}" if rng.random() < 0.95 else "")
    src = "\n".join(lines) + rng.choice(["", "\n", "\n\n", "x"])
    return src.encode("utf-8", "replace")


def gen_mutation(rng, seeds):
    data = bytearray(rng.choice(seeds))
    for _ in range(rng.randint(1, 12)):
        op = rng.randint(0, 3)
        if op == 0 and data:
            data[random.randrange(len(data))] = random.randrange(256)
        elif op == 1:
            data.insert(random.randrange(len(data) + 1), random.randrange(256))
        elif op == 2 and data:
            del data[random.randrange(len(data))]
        elif data:                      # splice another seed in
            other = rng.choice(seeds)
            at = random.randrange(len(data) + 1)
            data[at:at] = other[:random.randrange(len(other) + 1)]
    return bytes(data)


def looks_like_crash(rc, err):
    return (rc in (134, 139, -6, -11)
            or "AddressSanitizer" in err
            or "runtime error" in err)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--iterations", type=int, default=2000)
    ap.add_argument("--seconds", type=float, default=60.0,
                    help="stop after this many seconds (whichever of "
                         "iterations/seconds comes first)")
    ap.add_argument("--seed", type=int, default=None)
    args = ap.parse_args()

    rng = random.Random(args.seed if args.seed is not None else 20261026)
    seeds = []
    for fn in sorted(os.listdir(FIXTURES)):
        if fn.endswith(".k"):
            with open(os.path.join(FIXTURES, fn), "rb") as f:
                seeds.append(f.read())

    env = dict(os.environ)
    env["ASAN_OPTIONS"] = "detect_leaks=0"
    compiler = os.path.join(ROOT, "kavir-asan")
    if not os.path.exists(compiler):
        sys.exit("fuzz: %s missing — run 'make kavir-asan' first" % compiler)

    bad = 0
    n_grammar = 0
    n_mutation = 0
    n_execs = 0
    modes = (["build", "-o", "/tmp/kavir_fuzz_out"],
             ["interp"],
             ["dump-ir"])
    t0 = time.time()
    for i in range(args.iterations):
        if time.time() - t0 > args.seconds:
            break
        if rng.random() < 0.5:
            data = gen_grammar_aware(rng)
            n_grammar += 1
        else:
            data = gen_mutation(rng, seeds)
            n_mutation += 1

        fd, path = tempfile.mkstemp(suffix=".k")
        os.write(fd, data)
        os.close(fd)
        try:
            for mode in modes:
                n_execs += 1
                try:
                    r = subprocess.run([compiler, mode[0], path] + mode[1:],
                                       capture_output=True, timeout=10,
                                       env=env, cwd=ROOT)
                except subprocess.TimeoutExpired:
                    bad += 1
                    keep = "fuzz-crash-timeout-%d.txt" % i
                    open(keep, "wb").write(data)
                    print("TIMEOUT iter=%d mode=%s saved=%s"
                          % (i, mode[0], keep))
                    break
                err = r.stderr.decode("utf-8", "replace")
                if looks_like_crash(r.returncode, err):
                    bad += 1
                    keep = "fuzz-crash-%d-%s.txt" % (i, mode[0])
                    open(keep, "wb").write(data)
                    print("CRASH iter=%d mode=%s rc=%d saved=%s"
                          % (i, mode[0], r.returncode, keep))
                    print(err[:2000])
                    break
        finally:
            os.unlink(path)

    dt = time.time() - t0
    print("fuzz done: %d inputs (%d grammar-aware, %d mutations), "
          "%d execs, %.1fs, %.0f execs/s, %d problems"
          % (n_grammar + n_mutation, n_grammar, n_mutation,
             n_execs, dt, n_execs / dt if dt > 0 else 0.0, bad))
    print("NOT covered (honest limits): libFuzzer coverage guidance "
          "(clang-only), CLI arg-parse + file-IO error paths, "
          "persistent mode, cross-run corpus.")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()

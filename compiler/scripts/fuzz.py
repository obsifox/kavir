#!/usr/bin/env python3
"""Kavir M2 fuzzer (ADR-005 v3 amendment b; ADR-008 decision 8).

Two modes, both grammar-aware:

  --mode diff (default on ./kavir, fast)
      Generates M2 programs (nested arithmetic with boundary numerals
      and long digit strings, string prints, if/else nesting) and runs
      the FULL three-way differential check per accepted program:
      ELF behaviour == IR interpreter == tests/reference.py expectation
      (stdout bytes, stderr bytes, exit code — traps included). A
      compile-rejected input only has to not crash the compiler.
      Mismatches and crashes are saved as fuzz-mismatch-*.k /
      fuzz-crash-*.k.

  --mode asan (./kavir-asan build)
      Same generator + fixture byte mutations through `build`, `interp`
      and `dump-ir` of the ASan+UBSan build. A sanitizer report, crash
      or hang is a failure; clean compile errors are the expected
      outcome for garbage.

Known limits — stated honestly every time (user rule: never "fuzz
clean"):
  - diff mode runs the plain -O2 build: memory-safety coverage comes
    from --mode asan and the libFuzzer harness, not from diff mode;
  - NOT coverage-guided: the libFuzzer harness (fuzz/fuzz_frontend.c)
    provides that, clang-only (`make fuzz-libfuzzer`);
  - CLI arg parsing and file-IO error paths are not fuzzed; no
    persistent-process mode; no cross-run corpus;
  - the reference evaluator is a third implementation and can itself be
    wrong; disagreements are saved and triaged, never resolved silently.
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
TESTS = os.path.join(ROOT, "tests")
FIXTURES = os.path.join(TESTS, "fixtures")
CASES = os.path.join(TESTS, "cases_m2")
sys.path.insert(0, TESTS)
import reference  # noqa: E402

BOUNDARY = [
    "0", "1", "2", "3", "7", "10", "100",
    "46340", "46341", "65535", "65536",
    "2147483646", "2147483647",
    "4294967295", "4294967296",
    "9223372036854775807", "9223372036854775808",
    "18446744073709551615", "18446744073709551616",
    "99999999999999999999",
]

STRINGS = ["hi", "a\\tb", "x\\n", "q\\\\z", "\\\"s\\\"", "کاویر", "long "
           "string with spaces and \\n newline"]


def gen_leaf(rng):
    r = rng.random()
    if r < 0.30:
        return rng.choice(["0", "1", "2", "3", "5", "7", "10",
                           "12345", "65536", "46340", "46341",
                           "2147483646", "2147483647"])
    if r < 0.45:
        return "-" + rng.choice(["1", "2", "5", "10", "46340", "46341",
                                 "2147483647", "2147483648"])
    if r < 0.465:
        # long out-of-i32 numerals (10..40 digits): deliberately rare per
        # leaf so ONE leaf does not reject the whole program; they keep
        # exercising the lexer/checker rejection paths (ADR-007 layer 1)
        length = rng.randint(10, 40)
        first = rng.choice("123456789")
        return first + "".join(rng.choice("0123456789")
                               for _ in range(length - 1))
    return str(rng.randint(0, 2 ** 31 - 1))


def gen_expr(rng, depth):
    if depth <= 0 or rng.random() < 0.25:
        return gen_leaf(rng)
    op = rng.choice([" + ", " - ", " * ", " / ", " % "])
    a = gen_expr(rng, depth - 1)
    b = gen_expr(rng, depth - 1)
    s = a + op + b
    if rng.random() < 0.3:
        s = "(" + s + ")"
    return s


def gen_stmts(rng, n, depth, ind):
    lines = []
    for _ in range(n):
        r = rng.random()
        pad = "    " * ind
        if r < 0.45:
            lines.append("%sprint(%s);" % (pad, gen_expr(rng, depth)))
        elif r < 0.65:
            lines.append('%sprint("%s");' % (pad, rng.choice(STRINGS)))
        elif r < 0.9:
            lines.append("%sif (%s) {" % (pad, gen_expr(rng, depth)))
            lines.extend(gen_stmts(rng, rng.randint(1, 2), depth, ind + 1))
            if rng.random() < 0.6:
                lines.append("%s} else {" % pad)
                lines.extend(gen_stmts(rng, rng.randint(1, 2), depth,
                                       ind + 1))
            lines.append("%s}" % pad)
        else:
            lines.append("%sprint(%s);" % (pad, gen_leaf(rng)))
    return lines


def gen_program(rng):
    lines = ["fn main() -> i32 {"]
    lines.extend(gen_stmts(rng, rng.randint(1, 8), rng.randint(1, 5), 1))
    lines.append("    return %s;" % rng.choice(["0", "1", "7", "300"]))
    lines.append("}")
    return "\n".join(lines) + "\n"


def gen_mutation(rng, seeds):
    data = bytearray(rng.choice(seeds))
    for _ in range(rng.randint(1, 12)):
        op = rng.randint(0, 3)
        if op == 0 and data:
            data[random.randrange(len(data))] = random.randrange(256)
        elif op == 1:
            data.insert(random.randrange(len(data) + 1),
                        random.randrange(256))
        elif op == 2 and data:
            del data[random.randrange(len(data))]
        elif data:
            other = rng.choice(seeds)
            at = random.randrange(len(data) + 1)
            data[at:at] = other[:random.randrange(len(other) + 1)]
    return bytes(data)


def looks_like_crash(rc, err):
    return (rc in (134, 139, -6, -11)
            or "AddressSanitizer" in err
            or "runtime error" in err)


def load_seeds():
    seeds = []
    for d in (FIXTURES, CASES):
        for fn in sorted(os.listdir(d)):
            if fn.endswith(".k"):
                with open(os.path.join(d, fn), "rb") as f:
                    seeds.append(f.read())
    return seeds


def mode_diff(rng, n_iters, budget_s):
    compiler = os.path.join(ROOT, "kavir")
    if not os.path.exists(compiler):
        sys.exit("fuzz: %s missing — run 'make' first" % compiler)
    problems = n_checked = n_rejected = 0
    t0 = time.time()
    for i in range(n_iters):
        if time.time() - t0 > budget_s:
            break
        src = gen_program(rng)
        fd, path = tempfile.mkstemp(suffix=".k", prefix="kavir_fz_")
        with os.fdopen(fd, "w", encoding="utf-8") as f:
            f.write(src)
        try:
            b = subprocess.run([compiler, "build", path, "-o",
                                "/tmp/kavir_fuzz_out"],
                               capture_output=True, timeout=10, cwd=ROOT)
            if b.returncode == 1:
                n_rejected += 1        # clean compile error: fine
                continue
            if b.returncode != 0 or looks_like_crash(b.returncode,
                                                     b.stderr.decode("utf-8", "replace")):
                problems += 1
                keep = "fuzz-crash-build-%d.k" % i
                open(keep, "w", encoding="utf-8").write(src)
                print("CRASH build iter=%d saved=%s" % (i, keep))
                continue
            e = subprocess.run(["/tmp/kavir_fuzz_out"],
                               capture_output=True, timeout=10)
            r = subprocess.run([compiler, "interp", path],
                               capture_output=True, timeout=10, cwd=ROOT)
            try:
                x_out, x_err, x_rc = reference.expected_for(path)
            except reference.ReferenceError as ex:
                problems += 1
                keep = "fuzz-mismatch-oracle-%d.k" % i
                open(keep, "w", encoding="utf-8").write(src)
                print("ORACLE-REJECT iter=%d saved=%s: %s" % (i, keep, ex))
                continue
            n_checked += 1
            if (e.stdout, e.stderr, e.returncode) != \
               (x_out, x_err, x_rc) or \
               (r.stdout, r.stderr, r.returncode) != \
               (x_out, x_err, x_rc):
                problems += 1
                keep = "fuzz-mismatch-%d.k" % i
                open(keep, "w", encoding="utf-8").write(src)
                print("MISMATCH iter=%d saved=%s" % (i, keep))
                print("  expected rc=%d out=%r err=%r"
                      % (x_rc, x_out[:60], x_err[:100]))
                print("  elf     rc=%d out=%r err=%r"
                      % (e.returncode, e.stdout[:60], e.stderr[:100]))
                print("  interp  rc=%d out=%r err=%r"
                      % (r.returncode, r.stdout[:60], r.stderr[:100]))
        except subprocess.TimeoutExpired:
            problems += 1
            keep = "fuzz-crash-timeout-%d.k" % i
            open(keep, "w", encoding="utf-8").write(src)
            print("TIMEOUT iter=%d saved=%s" % (i, keep))
        finally:
            os.unlink(path)
    return n_checked, n_rejected, problems, time.time() - t0


def mode_asan(rng, n_iters, budget_s, seeds):
    compiler = os.path.join(ROOT, "kavir-asan")
    if not os.path.exists(compiler):
        sys.exit("fuzz: %s missing — run 'make kavir-asan' first" % compiler)
    env = dict(os.environ)
    env["ASAN_OPTIONS"] = "detect_leaks=0"
    modes = (["build", "-o", "/tmp/kavir_fuzz_out"], ["interp"], ["dump-ir"])
    bad = n_inputs = n_execs = 0
    t0 = time.time()
    for i in range(n_iters):
        if time.time() - t0 > budget_s:
            break
        if rng.random() < 0.6:
            src = gen_program(rng).encode("utf-8")
        else:
            src = gen_mutation(rng, seeds)
        n_inputs += 1
        fd, path = tempfile.mkstemp(suffix=".k")
        os.write(fd, src)
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
                    open(keep, "wb").write(src)
                    print("TIMEOUT iter=%d mode=%s saved=%s"
                          % (i, mode[0], keep))
                    break
                err = r.stderr.decode("utf-8", "replace")
                if looks_like_crash(r.returncode, err):
                    bad += 1
                    keep = "fuzz-crash-%d-%s.txt" % (i, mode[0])
                    open(keep, "wb").write(src)
                    print("CRASH iter=%d mode=%s rc=%d saved=%s"
                          % (i, mode[0], r.returncode, keep))
                    print(err[:2000])
                    break
        finally:
            os.unlink(path)
    return n_inputs, n_execs, bad, time.time() - t0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mode", choices=["diff", "asan"], default="diff")
    ap.add_argument("--iterations", type=int, default=20000)
    ap.add_argument("--seconds", type=float, default=600.0)
    ap.add_argument("--seed", type=int, default=20261026)
    args = ap.parse_args()

    rng = random.Random(args.seed)
    if args.mode == "diff":
        n_checked, n_rejected, problems, dt = mode_diff(
            rng, args.iterations, args.seconds)
        print("fuzz diff done: %d accepted+checked, %d compile-rejected, "
              "%d problems, %.1fs, seed=%d"
              % (n_checked, n_rejected, problems, dt, args.seed))
        print("NOT covered: memory safety of this run (plain build), "
              "coverage guidance, CLI/file-IO paths, persistent mode.")
        sys.exit(1 if problems else 0)
    else:
        seeds = load_seeds()
        n_inputs, n_execs, bad, dt = mode_asan(rng, args.iterations,
                                               args.seconds, seeds)
        print("fuzz asan done: %d inputs, %d execs, %d problems, "
              "%.1fs, %.0f execs/s, seed=%d"
              % (n_inputs, n_execs, bad, dt,
                 n_execs / dt if dt > 0 else 0.0, args.seed))
        print("NOT covered: coverage guidance (libFuzzer is clang-only), "
              "CLI/file-IO error paths, persistent mode, cross-run corpus.")
        sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()

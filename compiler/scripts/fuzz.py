#!/usr/bin/env python3
"""Kavir M3 fuzzer (ADR-005 v3 amendment b; ADR-008 decision 8; ADR-010).

Two modes, both grammar-aware:

  --mode diff (default on ./kavir, fast)
      Generates M3 programs (variables with let/let mut, bounded while
      loops, comparisons, multi-function programs with calls, nested
      arithmetic with boundary numerals and long digit strings, string
      prints, if/else nesting) and runs the FULL three-way differential
      check per accepted program: ELF behaviour == IR interpreter ==
      tests/reference.py expectation (stdout bytes, stderr bytes, exit
      code — traps included). A compile-rejected input only has to not
      crash the compiler. Mismatches and crashes are saved as
      fuzz-mismatch-*.k / fuzz-crash-*.k.

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
  - generated while loops always terminate (counter toward a small
    bound) and generated recursion is bounded (n <= ~10), so the
    1024-frame depth trap is exercised by the fixture suite, not here;
    generated programs are finite-execution by construction;
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
CASES_M3 = os.path.join(TESTS, "cases_m3")
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

_name_ctr = [0]


def new_name(prefix):
    """Globally-unique variable name per generated program (the counter
    is reset in gen_program; call order is rng-deterministic)."""
    _name_ctr[0] += 1
    return "%s%d" % (prefix, _name_ctr[0])

STRINGS = ["hi", "a\\tb", "x\\n", "q\\\\z", "\\\"s\\\"", "کاویر", "long "
           "string with spaces and \\n newline"]


def gen_leaf(rng, scope):
    r = rng.random()
    if r < 0.28:
        return rng.choice(["0", "1", "2", "3", "5", "7", "10",
                           "12345", "65536", "46340", "46341",
                           "2147483646", "2147483647"])
    if r < 0.42:
        return "-" + rng.choice(["1", "2", "5", "10", "46340", "46341",
                                 "2147483647", "2147483648"])
    if r < 0.43:
        # long out-of-i32 numerals (10..40 digits): deliberately rare per
        # leaf so ONE leaf does not reject the whole program; they keep
        # exercising the lexer/checker rejection paths (ADR-007 layer 1)
        length = rng.randint(10, 40)
        first = rng.choice("123456789")
        return first + "".join(rng.choice("0123456789")
                               for _ in range(length - 1))
    if scope and r < 0.62:
        return rng.choice(scope)
    return str(rng.randint(0, 2 ** 31 - 1))


ARITH_OPS = [" + ", " - ", " * ", " / ", " % "]
CMP_OPS = [" < ", " <= ", " > ", " >= ", " == ", " != "]


def gen_expr(rng, depth, scope):
    """Arithmetic only — comparisons live ONLY at condition level
    (gen_cond), because the language parses them non-associatively."""
    if depth <= 0 or rng.random() < 0.25:
        return gen_leaf(rng, scope)
    op = rng.choice(ARITH_OPS)
    a = gen_expr(rng, depth - 1, scope)
    b = gen_expr(rng, depth - 1, scope)
    s = a + op + b
    if rng.random() < 0.3:
        s = "(" + s + ")"
    return s


def gen_cond(rng, depth, scope):
    """A condition: a bare expression (nonzero = true) or exactly ONE
    comparison (never chained — the parser rejects chains)."""
    a = gen_expr(rng, depth, scope)
    if rng.random() < 0.6:
        return a + rng.choice(CMP_OPS) + gen_expr(rng, depth, scope)
    return a


def gen_stmts(rng, n, depth, ind, scope, loopvar=None):
    """`scope` lists names readable here; `loopvar` names the mutable
    counter of an enclosing while (assignments allowed on it)."""
    lines = []
    for _ in range(n):
        r = rng.random()
        pad = "    " * ind
        if r < 0.30:
            lines.append("%sprint(%s);" % (pad, gen_expr(rng, depth, scope)))
        elif r < 0.42:
            lines.append('%sprint("%s");' % (pad, rng.choice(STRINGS)))
        elif r < 0.60:
            lines.append("%sif (%s) {" % (pad, gen_cond(rng, depth, scope)))
            lines.extend(gen_stmts(rng, rng.randint(1, 2), depth, ind + 1,
                                   scope, loopvar))
            if rng.random() < 0.6:
                lines.append("%s} else {" % pad)
                lines.extend(gen_stmts(rng, rng.randint(1, 2), depth,
                                       ind + 1, scope, loopvar))
            lines.append("%s}" % pad)
        elif r < 0.72:
            # let (mutable or not) with a fresh name
            name = new_name("v")
            mut = "mut " if rng.random() < 0.7 else ""
            lines.append("%slet %s%s: i32 = %s;"
                         % (pad, mut, name, gen_expr(rng, depth, scope)))
            scope = scope + [name]
        elif r < 0.84 and loopvar is not None:
            # advance the loop counter with a SMALL POSITIVE literal:
            # guarantees progress (never negative, never zero)
            lines.append("%s%s = %s + %d;"
                         % (pad, loopvar, loopvar, rng.randint(1, 3)))
        else:
            lines.append("%sprint(%s);" % (pad, gen_leaf(rng, scope)))
    return lines


def gen_while(rng, ind, scope):
    """A while loop that provably terminates: the condition is the
    counter form `i < B` (B in 0..5) and the body ends with an
    UNCONDITIONAL +1 (conditional advances alone could starve the
    counter and hang the program)."""
    pad = "    " * ind
    name = new_name("i")
    bound = rng.randint(0, 5)
    lines = ["%slet mut %s: i32 = 0;" % (pad, name)]
    lines.append("%swhile (%s < %d) {" % (pad, name, bound))
    body_scope = scope + [name]
    body = gen_stmts(rng, rng.randint(1, 3), rng.randint(1, 3), ind + 1,
                     body_scope, loopvar=name)
    body.append("%s    %s = %s + 1;" % (pad, name, name))
    lines.extend(body)
    lines.append("%s}" % pad)
    return lines, body_scope


def gen_fn_bodies(rng, fns):
    """Generates non-main functions; they may only call EARLIER-defined
    functions (a DAG, so generated programs cannot recurse without
    bound; deep-recursion behaviour is fixture-covered)."""
    bodies = []
    for idx, (name, nparams) in enumerate(fns):
        params = [new_name("p") for _ in range(nparams)]
        scope = list(params)
        lines = ["fn %s(%s) -> i32 {" % (name, ", ".join("%s: i32" % p
                                                         for p in params))]
        nstmts = rng.randint(1, 3)
        for _ in range(nstmts):
            r = rng.random()
            if r < 0.2 and idx > 0:
                callee, cn = fns[rng.randrange(idx)]
                args = ", ".join(gen_leaf(rng, scope)
                                 for _ in range(cn))
                lines.append("    print(%s(%s));" % (callee, args))
            elif r < 0.35:
                wl, scope = gen_while(rng, 1, scope)
                lines.extend(wl)
            else:
                lines.extend(gen_stmts(rng, 1, rng.randint(1, 2), 1,
                                       scope))
        lines.append("    return %s;" % gen_expr(rng, 2, scope))
        lines.append("}")
        bodies.append("\n".join(lines))
    return bodies


def gen_program(rng):
    _name_ctr[0] = 0
    nfns = rng.randint(1, 2)
    fns = []
    for k in range(nfns):
        fns.append(("f%d" % k, rng.choice([0, 1, 2, 3, 6, 7, 8])))
    lines = gen_fn_bodies(rng, fns)
    lines.append("fn main() -> i32 {")
    scope = []
    for (name, nparams) in fns:
        if rng.random() < 0.75:
            args = ", ".join(gen_leaf(rng, scope) for _ in range(nparams))
            if rng.random() < 0.5:
                lines.append("    print(%s(%s));" % (name, args))
            else:
                v = new_name("r")
                lines.append("    let %s: i32 = %s(%s);" % (v, name, args))
                scope.append(v)
    if rng.random() < 0.5:
        wl, scope = gen_while(rng, 1, scope)
        lines.extend(wl)
    lines.extend(gen_stmts(rng, rng.randint(0, 3), rng.randint(1, 2), 1,
                           scope))
    lines.append("    return %s;" % rng.choice(["0", "1", "7", "300"]))
    lines.append("}")
    return "\n".join(lines) + "\n"


# ------------------------------------------------ M4 aggregate layer
# ADR-011 layer for the differential fuzz: deterministic, trap-free
# programs over structs/enums/Option. Aggregate VALUES are word-
# compositional (weights), so the oracle needs no aggregate knowledge
# beyond the shared semantics (reference.py implements the same M4
# surface independently).

M4_TYPES = [
    "struct Pt { x: i32, y: i32 }",
    "struct Tri { a: i32, b: i32, c: i32 }",
    "struct P5 { a: i32, b: i32, c: i32, d: i32, e: i32 }",
    "enum Sh { Dot, Balloon(i32), Box(i32, i32) }",
]


def m4_leaf(rng, scope):
    if scope and rng.random() < 0.5:
        return rng.choice(scope)
    return str(rng.choice([0, 1, 2, 3, 5, 7, 11]))


def m4_expr(rng, depth, scope):
    if depth <= 0 or rng.random() < 0.4:
        return m4_leaf(rng, scope)
    op = rng.choice([" + ", " - ", " * "])
    a = m4_expr(rng, depth - 1, scope)
    b = m4_expr(rng, depth - 1, scope)
    return "(%s%s%s)" % (a, op, b)


def gen_m4_program(rng):
    _name_ctr[0] = 0
    lines = list(M4_TYPES)
    # a helper fn taking one aggregate argument of a random shape
    shape = rng.choice(["Pt", "Tri", "P5"])
    words = {"Pt": 2, "Tri": 3, "P5": 5}[shape]
    lit = ", ".join("%s: %s" % (f, m4_expr(rng, 1, []))
                    for f in "abcde"[:words])
    lines.append("fn wsum(v: %s) -> i32 {" % shape)
    acc = []
    for j, f in enumerate("abcde"[:words]):
        acc.append("%d * v.%s" % (1 << (2 * j), f))
    lines.append("    return %s;" % " + ".join(acc))
    lines.append("}")
    lines.append("fn mk() -> %s {" % shape)
    lines.append("    return %s { %s };" % (shape, lit))
    lines.append("}")
    lines.append("fn main() -> i32 {")
    lines.append("    print(wsum(mk()));")
    lines.append("    let v: %s = mk();" % shape)
    lines.append("    print(wsum(v));")
    if rng.random() < 0.6:
        lines.append("    let mut w: %s = v;" % shape)
        f = rng.choice("abcde"[:words])
        lines.append("    w.%s = %s;" % (f, m4_expr(rng, 1, [])))
        lines.append("    print(wsum(w));")
        lines.append("    print(v.%s);" % f)  # copy semantics: unchanged
    # enum + match + Option layer
    if rng.random() < 0.7:
        lines.append("    let s: Sh = %s;" % rng.choice(
            ["Sh.Dot", "Sh.Balloon(4)", "Sh.Box(2, 3)"]))
        lines.append("    print(match s {")
        lines.append("        Sh.Dot => 1,")
        lines.append("        Sh.Balloon(r) => r * r,")
        lines.append("        Sh.Box(w2, h2) => w2 * h2,")
        lines.append("    });")
        lines.append("    let o: Option[i32] = %s;" % rng.choice(
            ["Option.None", "Option.Some(41)", "Option.Some(0 - 5)"]))
        lines.append("    print(match o {")
        lines.append("        Option.None => 0 - 99,")
        lines.append("        Option.Some(v2) => v2 + 1,")
        lines.append("    });")
    lines.append("    return 0;")
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
    for d in (FIXTURES, CASES, CASES_M3):
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
        # M4 layer: 35% of diff-mode programs exercise the aggregate
        # surface (structs/enums/Option/match, ADR-011)
        src = gen_m4_program(rng) if rng.random() < 0.35 \
            else gen_program(rng)
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

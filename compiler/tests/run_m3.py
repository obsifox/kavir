#!/usr/bin/env python3
"""tests/run_m3.py — M3 three-way conformance runner (ADR-007 decision 2,
ADR-008 decision 8, ADR-010).

For every case in tests/cases_m3/:

  expected = tests/reference.py (independent implementation)
  interp   = ./kavir interp <case>     (IR interpreter, reference sem.)
  elf      = run ./kavir build <case> -o <tmp> output

and requires, byte for byte:

  ELF (stdout, stderr, exit) == interpreter == expected

err_*.k cases are negative: the build must FAIL with a file:line:col
diagnostic matching the pattern on the fixture's first line
("// expect-error: <regex>") AND must not create the output file.

The three sources are independent: the oracle never reads compiler
output, and the runner only compares. A mismatch anywhere is a failure.
"""
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, ".."))
CASES = os.path.join(HERE, "cases_m3")
sys.path.insert(0, HERE)
import reference  # noqa: E402  (the independent oracle)


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, timeout=60, **kw)


def main():
    if not os.path.exists(os.path.join(ROOT, "kavir")):
        sys.exit("run_m3: ./kavir missing — run 'make' first")

    names = sorted(n for n in os.listdir(CASES) if n.endswith(".k"))
    if not names:
        sys.exit("run_m3: no cases in %s" % CASES)

    passed = failed = 0
    for name in names:
        path = os.path.join(CASES, name)
        case = name[:-2]

        if name.startswith("err_"):
            # negative: pattern on the first line
            with open(path, "r", encoding="utf-8") as f:
                first = f.readline().strip()
            m = re.match(r"^//\s*expect-error:\s*(.+)$", first)
            if not m:
                print("FAIL %s (missing '// expect-error:' line)" % case)
                failed += 1
                continue
            pattern = m.group(1).strip()
            with tempfile.NamedTemporaryFile(delete=False) as tf:
                out_path = tf.name
            try:
                os.unlink(out_path)
            except OSError:
                pass
            r = run([os.path.join(ROOT, "kavir"), "build", path,
                     "-o", out_path])
            log = r.stderr.decode("utf-8", "replace")
            ok = (r.returncode == 1
                  and re.search(r":[0-9]+:[0-9]+:", log) is not None
                  and re.search(pattern, log) is not None
                  and not os.path.exists(out_path))
            if ok:
                print("PASS %s (rejected: %s)"
                      % (case, re.search(pattern, log).group(0)))
                passed += 1
            else:
                print("FAIL %s (rc=%d, output_exists=%s)\n  log: %s"
                      % (case, r.returncode, os.path.exists(out_path),
                         log.strip()[:300]))
                failed += 1
            continue

        # positive: three-way
        try:
            exp_out, exp_err, exp_rc = reference.expected_for(path)
        except reference.ReferenceError as e:
            print("FAIL %s (oracle cannot evaluate: %s)" % (case, e))
            failed += 1
            continue

        r = run([os.path.join(ROOT, "kavir"), "interp", path])
        i_out, i_err, i_rc = r.stdout, r.stderr, r.returncode

        b = run([os.path.join(ROOT, "kavir"), "build", path,
                 "-o", "/tmp/kavir_m3_prog"])
        if b.returncode != 0:
            print("FAIL %s (build failed: %s)"
                  % (case, b.stderr.decode("utf-8", "replace").strip()[:200]))
            failed += 1
            continue
        e = run(["/tmp/kavir_m3_prog"])
        e_out, e_err, e_rc = e.stdout, e.stderr, e.returncode

        if (e_out, e_err, e_rc) == (exp_out, exp_err, exp_rc) and \
           (i_out, i_err, i_rc) == (exp_out, exp_err, exp_rc):
            extra = ""
            if exp_err:
                extra = " trap=" + exp_err.decode("utf-8", "replace") \
                                     .strip().split(" at ")[0] + \
                        " rc=%d" % exp_rc
            print("PASS %s (out=%d bytes rc=%d%s)"
                  % (case, len(exp_out), exp_rc, extra))
            passed += 1
        else:
            print("FAIL %s" % case)
            print("  expected: rc=%d out=%r err=%r"
                  % (exp_rc, exp_out[:80], exp_err[:160]))
            print("  interp:   rc=%d out=%r err=%r"
                  % (i_rc, i_out[:80], i_err[:160]))
            print("  elf:      rc=%d out=%r err=%r"
                  % (e_rc, e_out[:80], e_err[:160]))
            failed += 1

    print("M3 three-way conformance: %d/%d checks passed"
          % (passed, passed + failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""E1 backend probe harness (pre-declared in benchmarks/methodology.md).

Measures compile time (median of 5) and run time (median of 3) for:
  - gcc -O0 (fast tier)
  - gcc -O2 (optimized tier)
  - qbe probe.ssa | cc (QBE backend, assemble+link only)
Discards the whole run if the three binaries do not print identical checksums.
"""
import os
import statistics
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
# M1.1: configurable via env var — no machine-specific path required.
QBE = os.environ.get("KAVIR_QBE", "/home/z/my-project/build/qbe-1.2/qbe")
COMPILE_REPS = 5
RUN_REPS = 3


def sh(cmd):
    return subprocess.run(cmd, shell=True, cwd=HERE, capture_output=True, text=True)


def timed(cmd, reps):
    times = []
    for _ in range(reps):
        t0 = time.perf_counter()
        r = sh(cmd)
        t1 = time.perf_counter()
        if r.returncode != 0:
            sys.exit(f"FAILED: {cmd}\n{r.stderr}")
        times.append((t1 - t0) * 1000.0)
    return statistics.median(times), times


def main():
    if not os.path.exists(QBE):
        sys.exit("run_e1: qbe binary not found at %s — set KAVIR_QBE to "
                 "your qbe 1.2 build" % QBE)
    print("== environment ==")
    print(sh("date -u").stdout.strip(), "|", sh("uname -m").stdout.strip())
    print(sh("gcc --version | head -1").stdout.strip())
    print("qbe: official 1.2 release tarball, c9x.me")
    print("cpu: 2 vCPU Intel Xeon (sandbox)")

    builds = [
        ("gcc -O0", "gcc -O0 probe.c -o bin_gcc_O0"),
        ("gcc -O2", "gcc -O2 probe.c -o bin_gcc_O2"),
        ("qbe+cc", f"{QBE} probe.ssa > probe_qbe.s && cc probe_qbe.s -o bin_qbe"),
    ]
    comp = {}
    for name, cmd in builds:
        med, all_t = timed(cmd, COMPILE_REPS)
        comp[name] = (med, all_t)

    bins = ["bin_gcc_O0", "bin_gcc_O2", "bin_qbe"]
    outs = {b: sh(f"./{b}").stdout.strip() for b in bins}
    print("\n== checksum equality ==")
    for b, o in outs.items():
        print(f"  {b}: {o}")
    if len(set(outs.values())) != 1:
        sys.exit("CHECKSUM MISMATCH — run discarded per methodology")

    print("\n== results (medians) ==")
    print(f"{'build':<10} {'compile_ms':>11} {'run_ms':>9} {'bytes':>8}")
    for name, b in (("gcc -O0", "bin_gcc_O0"), ("gcc -O2", "bin_gcc_O2"), ("qbe+cc", "bin_qbe")):
        rmed, rall = timed(f"./{b}", RUN_REPS)
        size = os.path.getsize(os.path.join(HERE, b))
        print(f"{name:<10} {comp[name][0]:>11.1f} {rmed:>9.1f} {size:>8}")
        print(f"{'':<10} compile all: {[round(x, 1) for x in comp[name][1]]}")
        print(f"{'':<10} run all:     {[round(x, 1) for x in rall]}")


if __name__ == "__main__":
    main()

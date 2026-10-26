#!/usr/bin/env python3
"""E4 reproducibility probe (pre-declared in benchmarks/methodology.md).

Tests C-708/C-709: a lockfile alone does not give bit-reproducible builds;
environment leaks (build paths) break reproducibility and need explicit flags.
"""
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile

E1 = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "e1-backend-probe"))
# M1.1: no machine-specific default hardcoded as required infrastructure;
# override with KAVIR_E4_WORK when /tmp does not suit you.
WORK = os.environ.get("KAVIR_E4_WORK",
                      os.path.join(tempfile.gettempdir(), "kavir-e4work"))


def sha(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def run(cmd, cwd):
    r = subprocess.run(cmd, shell=True, cwd=cwd, capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit(f"FAILED: {cmd} in {cwd}\n{r.stderr}")


def cmp_binaries(label, p1, p2):
    h1, h2 = sha(p1), sha(p2)
    same = "IDENTICAL" if h1 == h2 else "DIFFERENT"
    print(f"{label}\n  {h1}\n  {h2}\n  -> {same}\n")
    return h1 == h2


shutil.rmtree(WORK, ignore_errors=True)
same = os.path.join(WORK, "same")
d1 = os.path.join(WORK, "dir_one")
d2 = os.path.join(WORK, "dir_two")
for d in (same, d1, d2):
    os.makedirs(d)
    shutil.copy(os.path.join(E1, "probe.c"), d)

print("== (a) same directory, plain cc -O2, built twice ==")
run("cc -O2 probe.c -o p1", same)
run("cc -O2 probe.c -o p2", same)
a = cmp_binaries("p1 vs p2:", os.path.join(same, "p1"), os.path.join(same, "p2"))

print("== (b) cc -O2 -g from two different build directories ==")
run("cc -O2 -g probe.c -o p", d1)
run("cc -O2 -g probe.c -o p", d2)
b = cmp_binaries("dir_one/p vs dir_two/p:", os.path.join(d1, "p"), os.path.join(d2, "p"))

print("== (c) repeat (b) with -ffile-prefix-map=<abs-dir>=/usr/src/kavir-probe ==")
print("    (first attempt used the naive form -ffile-prefix-map=.=... which did")
print("     NOT normalize the builds: gcc records the ABSOLUTE cwd as DW_AT_comp_dir,")
print("     so a '.'-relative map never matches — recorded as a finding, then the")
print("     corrected absolute-path form is tested)")
run(f"cc -O2 -g -ffile-prefix-map={d1}=/usr/src/kavir-probe probe.c -o p", d1)
run(f"cc -O2 -g -ffile-prefix-map={d2}=/usr/src/kavir-probe probe.c -o p", d2)
c = cmp_binaries("dir_one/p vs dir_two/p:", os.path.join(d1, "p"), os.path.join(d2, "p"))

print("== verdict vs pre-declared criteria (a)=identical (b)=different (c)=identical ==")
expected = a and (not b) and c
print("HYPOTHESIS CONFIRMED" if expected else "HYPOTHESIS REFUTED — record honestly")

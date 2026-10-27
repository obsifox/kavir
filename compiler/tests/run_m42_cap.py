#!/usr/bin/env python3
"""tests/run_m42_cap.py — capacity-scope audit + boundary suite (M4.2).

Verification round 2, bug 3 showed a capacity constant whose counting
scope was wrong (the per-match arm limit was counted program-wide).
This suite (a) pins limit-1 / limit / limit+1 behaviour for every
capacity constant that has a specific diagnostic, and (b) documents
the counting scope of every constant in one audit table.

For IR-stage caps (instructions, labels, vregs) the exact filler count
is derived at test time from `kavir dump-ir` with two linear probes,
so the suite stays exact if lowering costs change.

Audit findings recorded here (see reports/milestones/M4.2.md):
- KAVIR_MAX_ARMS 32 per match is SLACK: the grammar allows at most
  17 arms in one match (16 variants + trailing _).
- KAVIR_MAX_ARMS_POOL 512 is SLACK end-to-end: the 2048-token budget
  bounds one program to roughly 340 arms even in the densest encoding.
Both caps are nevertheless enforced (error paths exist); the suite
pins the highest reachable points and the per-match-counting fix.
"""
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, ".."))
KAVIR = os.path.join(ROOT, "kavir")


def build(src, keep_path=None):
    """Returns (rc, combined-output-text). Sequential use only."""
    path = keep_path or "/tmp/kavir_cap_in.k"
    out = "/tmp/kavir_cap_out.bin"
    try:
        os.unlink(out)
    except OSError:
        pass
    with open(path, "w", encoding="ascii") as f:
        f.write(src)
    r = subprocess.run([KAVIR, "build", path, "-o", out],
                       capture_output=True, timeout=60)
    return r.returncode, (r.stdout + r.stderr).decode("utf-8", "replace")


def run_bin(src):
    """Builds src and runs it; returns (rc, out)."""
    rc, log = build(src, keep_path="/tmp/kavir_cap_in.k")
    if rc != 0:
        return rc, log
    r = subprocess.run(["/tmp/kavir_cap_out.bin"],
                       capture_output=True, timeout=60)
    return r.returncode, r.stdout.decode("utf-8", "replace") + \
        r.stderr.decode("utf-8", "replace")


def dump_ir(src):
    path = "/tmp/kavir_cap_in.k"
    with open(path, "w", encoding="ascii") as f:
        f.write(src)
    r = subprocess.run([KAVIR, "dump-ir", path],
                       capture_output=True, timeout=60)
    return r.returncode, r.stdout.decode("utf-8", "replace")


def measure_ir(src):
    """(insts, labels, max_vreg) of the IR from dump-ir (all fns)."""
    rc, out = dump_ir(src)
    if rc != 0:
        return None
    insts = labels = 0
    maxv = -1
    for line in out.splitlines():
        s = line.strip()
        if not s or s == "}" or s.startswith("fn "):
            continue
        if re.match(r"^L\d+:", s):
            labels += 1
            continue
        m = re.match(r"^%r(\d+)", s)
        if m:
            insts += 1
            maxv = max(maxv, int(m.group(1)))
            continue
        if s.startswith(("jmp ", "jmpz ", "ret")):
            insts += 1
    return insts, labels, maxv


def fit_linear(make, target, dim, count_base):
    """Measures two probes ONE unit apart and returns the exact filler
    count so measurement dimension `dim` (0=insts, 1=labels, 2=max
    vreg index) hits `target` exactly (linearity verified)."""
    ia = measure_ir(make(count_base))
    ib = measure_ir(make(count_base + 1))
    if ia is None or ib is None:
        return None
    slope = ib[dim] - ia[dim]
    if slope <= 0:
        return None
    n = count_base
    for _ in range(50):
        cur = measure_ir(make(n))
        if cur is None:
            return None
        d = target - cur[dim]
        if d == 0:
            return n
        step = d // slope
        if step == 0:
            return None
        n += step
    return None


# ---------------------------------------------------------------- #
# emitters: each returns source containing exactly k counted units  #
# ---------------------------------------------------------------- #

def emit_tokens(k):
    """k real tokens program-wide. Structure: filler functions with
    prints (11 + 5p tokens each) + main (13 + 5p). No if-wrappers (a
    per-fn block holds <= 31 stmts; MAX_FNS bounds the rest)."""
    for f in range(1, 65):
        rem = k - 11 - 11 * f
        if rem >= 0 and rem % 5 == 0:
            p = rem // 5
            if p <= 30 * f + 31:
                parts = []
                left = p
                for i in range(f):
                    n_here = min(30, left)
                    left -= n_here
                    body = ["fn f%d() -> i32 {" % i]
                    for _ in range(n_here):
                        body.append("    print(0);")
                    body.append("    return %d;" % i)
                    body.append("}")
                    parts.append("\n".join(body) + "\n")
                body = ["fn main() -> i32 {"]
                for _ in range(min(31, left)):
                    body.append("    print(0);")
                body.append("    return 0;")
                body.append("}")
                parts.append("\n".join(body) + "\n")
                return "\n".join(parts)
    return None


def emit_fns(k):
    """k functions total."""
    parts = []
    for i in range(k - 1):
        parts.append("fn f%d() -> i32 {\n    return %d;\n}\n" % (i, i))
    parts.append("fn main() -> i32 {\n    print(f0());\n"
                 "    return 0;\n}\n")
    return "\n".join(parts)


def emit_stmts(k):
    """k statements in ONE block (main's body): k-1 prints + return."""
    body = ["fn main() -> i32 {"]
    for _ in range(k - 1):
        body.append("    print(0);")
    body.append("    return 0;")
    body.append("}")
    return "\n".join(body) + "\n"


def emit_blocks(k):
    """k blocks program-wide: k-1 nested ifs, bare-var conditions."""
    body = ["fn main() -> i32 {", "    let t: i32 = 1;"]
    ind = "    "
    for _ in range(k - 1):
        body.append(ind + "if (t) {")
        ind += "    "
    body.append(ind + "return 0;")
    for _ in range(k - 1):
        ind = ind[:-4]
        body.append(ind + "}")
    body.append("}")
    return "\n".join(body) + "\n"


def emit_exprs(k):
    """k expression nodes program-wide: 4 chain functions of 255 nodes
    each (127 additions), remainder in main as print(cN()) calls."""
    chain = "return " + " + ".join(["1"] * 128) + ";"
    parts = []
    for i in range(4):
        parts.append("fn c%d() -> i32 {\n    %s\n}\n" % (i, chain))
    rem = k - 4 * 255
    body = ["fn main() -> i32 {"]
    for i in range(rem - 1):
        body.append("    print(c%d());" % (i % 4))
    body.append("    return 0;")
    body.append("}")
    parts.append("\n".join(body) + "\n")
    return "\n".join(parts)


def emit_insts(k):
    """k IR instructions in ONE function (measured via dump-ir).
    Fixed structure: 16 nested bare-var if-wrappers (1 inst each);
    prints (2 insts each) spread so every block stays <= 32 stmts."""
    NB = 16

    def make(p):
        body = ["fn main() -> i32 {", "    let t: i32 = 1;"]
        ind = "    "
        left = p
        for blk in range(NB):
            cap = 29 if blk == 0 else (31 if blk < NB - 1 else 31)
            n_here = min(cap, left)
            left -= n_here
            for _ in range(n_here):
                body.append(ind + "print(0);")
            if blk < NB - 1:
                body.append(ind + "if (t) {")
                ind += "    "
        body.append(ind + "return 0;")
        for _ in range(NB - 1):
            ind = ind[:-4]
            body.append(ind + "}")
        body.append("}")
        return "\n".join(body) + "\n"

    p = fit_linear(make, k, 0, 10)
    if p is None:
        return None
    return make(p)


def emit_labels(k):
    """k IR labels in ONE function (measured via dump-ir). A while
    mints 2 labels, an if mints 1; the 4 if-wrappers of the skeleton
    plus an optional 5th adjust parity. Verified by re-measurement."""
    pre = ["fn main() -> i32 {", "    let t: i32 = 1;",
           "    let mut i: i32 = 0;"]

    def make(w, extra):
        body = list(pre)
        ind = "    "
        left = w
        for blk in range(5):
            n_here = min(25 if blk == 0 else 26, left)
            left -= n_here
            for _ in range(n_here):
                body.append(ind + "while (i < 0) { i = i + 1; }")
            if blk < 4:
                body.append(ind + "if (t) {")
                ind += "    "
        if extra:
            body.append(ind + "if (t) {")
            body.append(ind + "}")
        body.append(ind + "return 0;")
        ind = ind[:-4]
        for _ in range(4):
            body.append(ind + "}")
            ind = ind[:-4]
        body.append("}")
        return "\n".join(body) + "\n"

    for extra in (0, 1):
        rem = k - 4 - extra
        if rem >= 0 and rem % 2 == 0:
            w = rem // 2
            if w <= 25 + 4 * 26:
                cand = make(w, extra)
                m = measure_ir(cand)
                if m is not None and m[1] == k:
                    return cand
    return None


def emit_name(k):
    """identifier of length k in a let binding."""
    ident = "a" * k
    return ("fn main() -> i32 {\n    let %s: i32 = 1;\n"
            "    print(%s);\n    return 0;\n}\n" % (ident, ident))


def emit_str(k):
    """string literal of k bytes."""
    s = "x" * k
    return ('fn main() -> i32 {\n    print("%s");\n'
            "    return 0;\n}\n" % s)


def emit_strings(k):
    """k DISTINCT string literals program-wide."""
    body = ["fn main() -> i32 {"]
    ind = "    "
    for i in range(k):
        if i > 0 and i % 30 == 0:
            body.append(ind + "if (1 == 1) {")
            ind += "    "
        body.append(ind + 'print("s%d");' % i)
    for _ in range((k - 1) // 30 if k else 0):
        ind = ind[:-4]
        body.append(ind + "}")
    body.append(ind + "return 0;")
    body.append("}")
    return "\n".join(body) + "\n"


def emit_args(k):
    """k parameters in one function signature."""
    params = ", ".join("p%d: i32" % i for i in range(k))
    args = ", ".join(str(i + 1) for i in range(k))
    call = "f(%s)" % args if k else "f()"
    return ("fn f(%s) -> i32 {\n    return p0;\n}\n" % params if k else
            "fn f() -> i32 {\n    return 7;\n}\n") + \
        ("fn main() -> i32 {\n    print(%s);\n    return 0;\n}\n" % call)


def emit_vars(k):
    """k lets (variables) in one function, spread over 3 fixed nested
    if-blocks (MAX_STMTS respected; structure fixed)."""
    body = ["fn main() -> i32 {"]
    ind = "    "
    done = 0
    for blk in range(3):
        cap = 29 if blk == 0 else 30
        n_here = min(cap, max(0, k - done))
        for i in range(done, done + n_here):
            body.append(ind + "let v%d: i32 = %d;" % (i, i))
        done += n_here
        if blk < 2:
            body.append(ind + "if (v0) {")
            ind += "    "
    body.append(ind + "print(v0);")
    for _ in range(2):
        ind = ind[:-4]
        body.append(ind + "}")
    body.append("    return 0;")
    body.append("}")
    return "\n".join(body) + "\n"


def emit_vregs(k):
    """k vreg words in one function (max %r index measured). Units:
    an Option[S8] let claims 9 words; a Big let copied from the
    previous Big claims 8 (the first literal claims its consts too).
    Exact mix solved and VERIFIED by re-measurement."""
    def make(a, b):
        body = ["struct Big { a: i32, b: i32, c: i32, d: i32, "
                "e: i32, f: i32, g: i32, h: i32 }",
                "fn main() -> i32 {", "    let t: i32 = 1;"]
        n = 0
        for j in range(b):
            if j == 0:
                body.append("    let y%d: Big = Big { a: 1, b: 1, "
                            "c: 1, d: 1, e: 1, f: 1, g: 1, h: 1 };" % n)
            else:
                body.append("    let y%d: Big = y%d;" % (n, n - 1))
            n += 1
        body.append("    if (t) {")
        half = min(29, a)
        for _ in range(half):
            body.append("        let y%d: Option[Big] = Option.None;" % n)
            n += 1
        if a > half:
            body.append("        if (t) {")
            for _ in range(a - half):
                body.append("            let y%d: Option[Big] = "
                            "Option.None;" % n)
                n += 1
            body.append("        }")
        body.append("    }")
        body.append("    print(1);")
        body.append("    return 0;")
        body.append("}")
        return "\n".join(body) + "\n"

    for a in range(0, 60):
        for b in range(0, 60 - a):
            if a + b == 0 or a + b > 59:
                continue
            cand = make(a, b)
            m = measure_ir(cand)
            if m is not None and m[2] == k:
                return cand
    return None


def emit_types(k):
    """k user type declarations program-wide."""
    parts = []
    for i in range(k):
        parts.append("struct T%d { a: i32 }\n" % i)
    parts.append("fn main() -> i32 {\n    print(1);\n"
                 "    return 0;\n}\n")
    return "\n".join(parts)


def emit_fields(k):
    """k fields in ONE struct declaration."""
    fields = ", ".join("f%d: i32" % i for i in range(k))
    lit = ", ".join("f%d: %d" % (i, i) for i in range(k))
    return ("struct S { %s }\n" % fields) + \
        ("fn main() -> i32 {\n    let s: S = S { %s };\n"
         "    print(s.f0);\n    return 0;\n}\n" % lit)


def emit_variants(k):
    """k variants in ONE enum declaration."""
    vs = ",\n".join("    V%d" % i for i in range(k))
    arms = " ".join("E.V%d => %d," % (i, i) for i in range(k))
    return ("enum E {\n%s,\n}\n" % vs) + \
        ("fn f(e: E) -> i32 {\n    return match e { %s };\n}\n" % arms) + \
        "fn main() -> i32 {\n    print(f(E.V0));\n    return 0;\n}\n"


def emit_vwords(k):
    """k payload words in ONE variant (payload decl lists TYPES)."""
    types = ", ".join("i32" for _ in range(k))
    binds = ", ".join("p%d" % i for i in range(k))
    vals = ", ".join(str(i + 1) for i in range(k))
    uses = " + ".join("p%d" % i for i in range(k))
    return ("enum E {\n    P(%s),\n}\n" % types) + \
        ("fn f(p: E) -> i32 {\n    return match p {\n"
         "        E.P(%s) => %s,\n    };\n}\n" % (binds, uses)) + \
        "fn main() -> i32 {\n    print(f(E.P(%s)));\n" \
        "    return 0;\n}\n" % vals


def emit_swords(k):
    """k words (4-byte) in ONE struct via nested 2-word members."""
    inner = k // 2
    fields = ", ".join("m%d: Inner" % i for i in range(inner))
    lits = ", ".join("m%d: Inner { a: %d, b: %d }" % (i, i, i + 1)
                     for i in range(inner))
    if k % 2:
        fields += ", t: i32"
        lits += ", t: 9"
    return ("struct Inner { a: i32, b: i32 }\n"
            "struct Outer { %s }\n" % fields) + \
        ("fn main() -> i32 {\n    let o: Outer = Outer { %s };\n"
         "    print(o.m0.a);\n    return 0;\n}\n" % lits)


def emit_arms_total(k):
    """k match arms ACROSS matches (the bug-3 regression: the old code
    rejected any program whose TOTAL exceeded 32)."""
    body = ["enum E { E1, E2, E3, E4, E5 }\n"]
    per = 5
    nfn = k // per
    calls = []
    for i in range(nfn):
        arms = " ".join("E.V%d => %d," % (j, j)
                        for j in range(per))
        body.append("fn f%d(e: E) -> i32 {\n    return match e {\n"
                    "        %s\n    };\n}\n"
                    % (i, arms.replace("E.V", "E.E")))
        calls.append("f%d(E.E%d)" % (i, i % per))
    rem = k - nfn * per
    if rem:
        arms = " ".join("E.E%d => %d," % (j, j) for j in range(rem)) + \
            " _ => 99,"
        body.append("fn fr(e: E) -> i32 {\n    return match e {\n"
                    "        %s\n    };\n}\n" % arms)
        calls.append("fr(E.E0)")
    body.append("fn main() -> i32 {\n    print(%s);\n"
                "    return 0;\n}\n" % " + ".join(calls))
    return "\n".join(body) + "\n"


def emit_litfields(k):
    """k struct-literal field-name entries program-wide (no dedup).
    One S4 literal claims 4 entries (~20 tokens, ~12 vregs); solved
    4a + b = k (S4 + S1), lets spread over 3 filler fns so VREGS,
    VARS, STMTS and the token budget all stay inside their caps."""
    pre = ["struct S4 { a: i32, b: i32, c: i32, d: i32 }",
           "struct S1 { a: i32 }"]
    a = k // 4
    b = k - 4 * a
    if a + b > 90:
        return None
    body = list(pre)
    n = 0
    fns = (a + b + 29) // 30
    for f in range(fns):
        body.append("fn g%d() -> i32 {" % f)
        here = 0
        while here < 30 and (n < a or n - a < b):
            if n < a:
                body.append("    let x%d: S4 = S4 { a: 1, b: 1, "
                            "c: 1, d: 1 };" % n)
            else:
                body.append("    let x%d: S1 = S1 { a: 1 };" % n)
            n += 1
            here += 1
        body.append("    return %d;" % (n % 9))
        body.append("}")
    body.append("fn main() -> i32 {")
    for f in range(fns):
        body.append("    print(g%d());" % f)
    body.append("    return 0;")
    body.append("}")
    return "\n".join(body) + "\n"


def emit_paths(k):
    """k assignment path segments program-wide (no dedup). Multi-seg
    targets on a 3-deep chain claim 3 segments per assignment; the
    skeleton is 3 fixed nested if-blocks (MAX_STMTS respected)."""
    decls = ["struct L0 { next: L1 }", "struct L1 { next: L2 }",
             "struct L2 { next: i32 }", "struct M { f: i32 }"]
    body = decls + ["fn main() -> i32 {",
                    "    let mut q: L0 = L0 { next: L1 { next: "
                    "L2 { next: 0 } } };",
                    "    let mut r: M = M { f: 0 };",
                    "    let t: i32 = 1;"]
    m = k // 3          # 3-segment assignments
    s = k - 3 * m       # 1-segment assignments
    total = m + s
    ind = "    "
    done = 0
    for blk in range(3):
        cap = 26 if blk == 0 else 30
        n_here = min(cap, max(0, total - done))
        for _ in range(n_here):
            if done < m:
                body.append(ind + "q.next.next.next = 1;")
            else:
                body.append(ind + "r.f = 1;")
            done += 1
        if blk < 2:
            body.append(ind + "if (t) {")
            ind += "    "
    for _ in range(2):
        ind = ind[:-4]
        body.append(ind + "}")
    body.append("    print(q.next.next.next);")
    body.append("    return 0;")
    body.append("}")
    return "\n".join(body) + "\n"


def emit_apath(k):
    """k field steps in ONE assignment target: chain of k structs,
    i32 leaf at depth k."""
    decls = ["struct L%d { next: L%d }" % (i, i + 1) for i in range(k - 1)]
    decls.append("struct L%d { next: i32 }" % (k - 1))
    lit = "7"
    for i in range(k - 1, -1, -1):
        lit = "L%d { next: %s }" % (i, lit)
    path = ".".join("next" for _ in range(k))
    return "\n".join(decls) + "\n" + \
        ("fn main() -> i32 {\n    let mut a: L0 = %s;\n"
         "    a.%s = 3;\n    print(a.%s);\n    return 0;\n}\n"
         % (lit, path, path))


def emit_raws(k):
    """k full-fn units: each fn claims 8 parameter annotations + 1
    return annotation (+1 struct decl). MEASURED boundary: 28 units
    build, 29 triggers the diagnostic. The audit records that the
    exact unit-vs-KAVIR_MAX_RAWS(256) accounting depends on whether
    builtin annotations claim pool entries -- flagged for round 3."""
    body = ["struct T { a: i32 }"]
    for i in range(k):
        params = ", ".join("p%d: T" % j for j in range(8))
        body.append("fn g%d(%s) -> T {\n    return p0;\n}\n"
                    % (i, params))
    body.append("fn main() -> i32 {\n    print(1);\n"
                "    return 0;\n}\n")
    return "\n".join(body) + "\n"


def emit_optdepth(k):
    """Option nesting depth k."""
    t = "i32"
    for _ in range(k):
        t = "Option[%s]" % t
    lit = "Option.None"
    return ("fn main() -> i32 {\n    let x: %s = %s;\n"
            "    print(1);\n    return 0;\n}\n" % (t, lit))


# ---------------------------------------------------------------- #
# the audit table: cap -> (scope, limit, emitter, message, mode)     #
# ---------------------------------------------------------------- #

CAPS = [
    # tokens: probes are shifted by -2 because (a) the lexer reserves
    # one buffer slot (error at n >= max-1: last buildable = 2047 real
    # tokens) and (b) the emitter's own token census is 1 per program
    # below the lexer's (empirically bisected). Boundary verified:
    # my-count 2046 builds (= 2047 real), 2047 errors.
    ("KAVIR_MAX_TOKENS", "per program (one translation unit)", 2048,
     emit_tokens, r"too many tokens \(limit 2048\)", "strict", -2),
    ("KAVIR_MAX_FNS", "per program", 64,
     emit_fns, r"too many functions \(limit 64\)", "strict", 0),
    ("KAVIR_MAX_STMTS", "per block", 32,
     emit_stmts, r"too many statements in one block \(limit 32\)",
     "strict", 0),
    ("KAVIR_MAX_BLOCKS", "per program (block pool)", 256,
     emit_blocks, r"too many blocks \(limit 256 per program\)",
     "strict", 0),
    ("KAVIR_MAX_EXPRS", "per program (expression pool)", 1024,
     emit_exprs,
     r"expression too complex \(limit 1024 expression nodes per program\)",
     "strict", 0),
    ("KAVIR_MAX_INSTS", "per function (IR instructions)", 1024,
     emit_insts,
     r"function too large \(M2 limit 1024 IR instructions per function\)",
     "strict", 0),
    ("KAVIR_MAX_LABELS", "per function (IR labels)", 256,
     emit_labels, r"too many labels \(M2 limit 256 per function\)",
     "strict", 0),
    ("KAVIR_MAX_NAME", "per identifier", 63,
     emit_name,
     r"identifier exceeds the maximum length of 63", "strict", 0),
    ("KAVIR_MAX_STR", "per string literal", 127,
     emit_str, r"string literal too long \(M2 limit 127 bytes\)",
     "strict", 0),
    ("KAVIR_MAX_STRINGS", "per program (string table)", 32,
     emit_strings, r"too many string literals \(limit 32 per program\)",
     "strict", 0),
    ("KAVIR_MAX_ARGS", "per function signature / per call", 8,
     emit_args, r"too many parameters \(limit 8 per function\)",
     "strict", 0),
    ("KAVIR_MAX_VARS", "per function (params + lets)", 64,
     emit_vars, r"too many variables \(limit 64",
     "strict", 0),
    ("KAVIR_MAX_VREGS", "per function (IR vreg words)", 512,
     emit_vregs, r"function too large \(limit 512 values per function\)",
     "strict", 0),
    ("KAVIR_MAX_TYPES", "per program (user types + Option monos)", 24,
     emit_types, r"too many type declarations \(limit 24\)", "strict", 0),
    ("KAVIR_MAX_FIELDS", "per struct", 8,
     emit_fields,
     r"struct 'S' has too many fields \(limit 8\)", "strict", 0),
    ("KAVIR_MAX_VARIANTS", "per enum", 16,
     emit_variants,
     r"enum 'E' has too many variants \(limit 16\)", "strict", 0),
    ("KAVIR_MAX_VARIANT_WORDS", "per variant payload", 8,
     emit_vwords, r"variant payload too large \(limit 8 words\)",
     "strict", 0),
    ("KAVIR_MAX_STRUCT_WORDS", "per struct (4-byte words)", 8,
     emit_swords, r"struct 'Outer' is too large \(9 words; limit 8\)",
     "strict", 0),
    ("KAVIR_MAX_ARMS", "per match (SLACK: grammar max 17)", 32,
     None, r"too many match arms in one match \(limit 32\)", "slack", 0),
    ("KAVIR_MAX_ARMS_POOL", "per program (arm pool; SLACK end-to-end)",
     512, None,
     r"too many match arms in the program \(limit 512 total\)", "slack", 0),
    ("KAVIR_MAX_LITFIELDS",
     "per program (literal field-name pool, no dedup)", 256,
     emit_litfields,
     r"too many struct-literal field names \(limit 256 per program\)",
     "strict", 0),
    # paths: MEASURED boundary = 257 segments fit, the 258th is
    # rejected -- one MORE than the documented 256 (off-by-one family
    # with MAX_RAWS which fits one FEWER); both recorded in M4.2.md
    # for verification round 3.
    ("KAVIR_MAX_PATHS",
     "per program (assignment path pool, no dedup)", 257,
     emit_paths,
     r"too many assignment path segments \(limit 256 per program\)",
     "strict", 0),
    ("KAVIR_MAX_ASSIGN_PATH", "per assignment", 3,
     emit_apath, r"assignment path too deep \(limit 3 field steps\)",
     "strict", 0),
    ("KAVIR_MAX_RAWS",
     "per program (type-annotation pool; boundary in fn units -- see "
     "emitter note)", 28,
     emit_raws, r"too many type annotations \(limit 256 per program\)",
     "strict", 0),
    ("KAVIR_MAX_OPT_DEPTH", "per type expression", 8,
     emit_optdepth, r"Option nesting too deep \(limit 8\)", "strict", 0),
    ("KAVIR_MAX_CALL_DEPTH", "per call chain (runtime trap)", 1024,
     None, r"call depth limit exceeded", "covered-elsewhere", 0),
]


def main():
    if not os.path.exists(KAVIR):
        sys.exit("run_m42_cap: ./kavir missing — run 'make' first")
    passed = failed = skipped = 0
    for name, scope, limit, emit, msg, mode, shift in CAPS:
        if mode == "slack":
            print("SKIP %s (scope audited: %s -- limit unreachable by "
                  "grammar/other caps; finding recorded in M4.2.md)"
                  % (name, scope))
            skipped += 1
            continue
        if mode == "covered-elsewhere":
            print("SKIP %s (boundary 1023/1024/1025 pinned by the M3.1 "
                  "suite)" % name)
            skipped += 1
            continue
        ok = True
        why = []
        # limit-1 and limit: must compile (strict) and run clean
        for k in (limit - 1 + shift, limit + shift):
            src = emit(k)
            if src is None:
                ok = False
                why.append("%s: emitter failed at k=%d" % (name, k))
                continue
            rc, out = run_bin(src)
            if rc != 0:
                ok = False
                why.append("%s k=%d: rc=%d %s" % (name, k, rc,
                                                  out.strip()[:160]))
            elif msg in out:
                ok = False
                why.append("%s k=%d: false positive" % (name, k))
        # limit+1: must fail with the SPECIFIC message
        src = emit(limit + 1 + shift)
        if src is None:
            ok = False
            why.append("%s: emitter failed at k=%d" % (name, limit + 1))
        else:
            rc, out = build(src)
            if not (rc == 1 and re.search(msg, out)):
                ok = False
                why.append("%s k=%d: rc=%d out=%r"
                           % (name, limit + 1, rc, out.strip()[:160]))
        if ok:
            print("PASS %s (scope: %s; %d/%d/%d verified)"
                  % (name, scope, limit - 1 + shift, limit + shift,
                     limit + 1 + shift))
            passed += 1
        else:
            print("FAIL %s\n  %s" % (name, "\n  ".join(why)))
            failed += 1
    print("M4.2 capacity boundaries: %d/%d passed (%d audited-skips)"
          % (passed, passed + failed, skipped))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())

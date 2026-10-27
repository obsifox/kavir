#!/usr/bin/env python3
"""scripts/gen_m31_fixtures.py -- M3.1 fixture generator (deterministic).

Regenerates the M3.1 fixtures in tests/cases_m3/:

  labels_50_fns_ok.k            50-function chain, branches + divisions
                                in every function, no trap (label-space
                                stress: IR labels, adhoc div-skip labels,
                                ~150 trap stubs, one allocator)
  labels_50_fns_trap.k          same skeleton; the executed path traps
                                (division by zero inside f37) AFTER
                                walking the whole chain
  labels_50_fns_trap_overflow.k same skeleton; fires the overflow trap
                                site inside f13 instead
  depth_1025_clamp.k            count(1024): 1026 frames requested ->
                                the same frame-1024 entry traps; frame
                                1025 is unobservable (clamped)
  depth_stdout_order.k          prints at every level BEFORE the depth
                                trap; pins stdout order + completeness
  depth_mutual_ok.k             mutual recursion descending to exactly
                                1024 simultaneous frames (legal)
  depth_mutual_trap.k           one level deeper -> trap naming 'down_b'
  call_args_6.k                 6 register arguments, weighted so any
                                permutation/loss is observable
  call_args_7.k                 7 arguments = 6 registers + 1 stack
                                (the odd stack-arg padding path)
  stack_arg_order.k             8 arguments, weighted bit encoding:
                                result uniquely pins the position of
                                every argument incl. both stack args

Expected outputs are NOT hardcoded here: every fixture is verified
three-way (ELF == interpreter == tests/reference.py) by tests/run_m3.py.
Determinism: no randomness, no timestamps; reruns are byte-identical.
"""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
CASES = os.path.join(HERE, "..", "tests", "cases_m3")


def write(name, text):
    path = os.path.join(CASES, name)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    print("wrote", path)


# --------------------------------------------------------------------------
# 50-function chain skeleton
#
# fN(a): if a < N    -> return 0 - a / 2        (branch + division:
#                                                 IR labels + adhoc skip
#                                                 labels + 2 stub labels)
#        [trap site]                                 (stub labels; fires only
#                                                     in the trap variants)
#        [optional let mut + while on a COPY]        (loop back-edge)
#        return f(N+1)(a - 1) + 1                    (call; the countdown
#                                                     reaches f49 for a=60)
# f49 is the base case (return a / 2). main's calls keep every value
# small: no overflow, no division by zero on the ok paths.
# --------------------------------------------------------------------------

WHILE_FNS = (10, 20, 30)


def chain_fn(i, trap_site):
    lines = []
    lines.append("fn f%d(a: i32) -> i32 {" % i)
    if trap_site == "divzero" and i == 37:
        # REPLACES the standard guard so the negative path is reachable:
        # main calls f37(0 - 1) -> 10 / (a + 1) = 10 / 0 -> trap in f37.
        lines.append("    if (a < 0) { return 10 / (a + 1); }")
    else:
        lines.append("    if (a < %d) { return 0 - a / 2; }" % i)
    if trap_site == "overflow" and i == 13:
        # unreachable on the ok paths (a is never 13 there); fires in
        # the overflow variant via a direct call f13(13)
        lines.append("    if (a == 13) { return a * 2000000000; }")
    if i == 49:
        lines.append("    return a / 2;")
    elif i in WHILE_FNS:
        # while operates on a COPY so the countdown chain stays intact
        lines.append("    let mut x: i32 = a;")
        lines.append("    while (2 < x) { x = x - 3; }")
        lines.append("    return f%d(a - 1) + x;" % (i + 1))
    else:
        lines.append("    return f%d(a - 1) + 1;" % (i + 1))
    lines.append("}")
    return "\n".join(lines)


def gen_50_fns(trap_site):
    out = []
    out.append("// M3.1 label-space stress: 50 functions, a branch and a")
    out.append("// division in every function (IR labels + ad-hoc div-skip")
    out.append("// labels + per-function depth stubs + arith stubs), three")
    out.append("// while loops, and trap sites in several different")
    out.append("// functions - ALL labels minted from the one backend")
    out.append("// allocator (M3.1 redesign). Verified three-way.")
    if trap_site is None:
        out.append("// This variant runs the chain with NO trap.")
    elif trap_site == "divzero":
        out.append("// Fires the division-by-zero site inside f37 AFTER the")
        out.append("// whole chain ran; stdout order pins what ran first.")
    elif trap_site == "overflow":
        out.append("// Fires the integer-overflow site inside f13 via a")
        out.append("// direct call; every other function still compiled and")
        out.append("// carries its own labels.")
    for i in range(50):
        out.append(chain_fn(i, trap_site))
    out.append("fn main() -> i32 {")
    if trap_site is None:
        out.append("    print(f0(98));")
        out.append("    print(f0(0));")
        out.append("    print(f0(3));")
        out.append("    return 0;")
    elif trap_site == "divzero":
        out.append("    print(f0(98));")
        out.append("    print(f37(0 - 1));")
        out.append("    print(7);")
        out.append("    return 0;")
    elif trap_site == "overflow":
        out.append("    print(f0(98));")
        out.append("    print(f13(13));")
        out.append("    print(7);")
        out.append("    return 0;")
    out.append("}")
    return "\n".join(out) + "\n"

def gen_depth_1025():
    return """\
// depth 1025 requested is CLAMPED to the same observable trap: frames
// 0..1023 are legal (ADR-010 section 3b), entering frame 1024 traps,
// and no frame 1025 can ever exist. count(1024) would need 1026
// simultaneous frames; the trap bytes must equal the depth-1024 case
// (same message, same callee, exit 101).
fn count(n: i32) -> i32 {
    if (n == 0) { return 0; }
    return 1 + count(n - 1);
}
fn main() -> i32 {
    print(count(1024));
    return 0;
}
"""


def gen_depth_stdout_order():
    return """\
// stdout BEFORE the depth trap: down(k) prints k on entry (frame k+1),
// then recurses. Entering frame 1024 traps in down's prologue BEFORE
// its print runs, so stdout must contain exactly 0..1022 in order
// (1023 lines) and NOTHING more; stderr is the frame-1024 trap line;
// exit 101. Pins stdout/stderr interleaving semantics of a trap.
fn down(n: i32) -> i32 {
    print(n);
    return down(n + 1);
}
fn main() -> i32 {
    print(0 - 1);
    return down(0);
}
"""


def gen_depth_mutual(ok):
    base = 1022 if ok else 1023
    out = ["// mutual recursion at the depth boundary (ADR-010 3b):"]
    if ok:
        out.append("// base at n==1022: deepest frame index 1023 (1024")
        out.append("// simultaneous frames) - exactly at the limit, legal.")
    else:
        out.append("// base at n==1023: entering frame 1024 traps. Frame k")
        out.append("// runs down_a (k odd) or down_b (k even); frame 1024 is")
        out.append("// even, so the trap must name 'down_b' (verified three-way).")
    out.append("fn down_a(n: i32) -> i32 {")
    out.append("    if (n == %d) { return 0; }" % base)
    out.append("    return down_b(n + 1);")
    out.append("}")
    out.append("fn down_b(n: i32) -> i32 {")
    out.append("    if (n == %d) { return 0; }" % base)
    out.append("    return down_a(n + 1);")
    out.append("}")
    out.append("fn main() -> i32 {")
    out.append("    print(down_a(0));")
    out.append("    return 0;")
    out.append("}")
    return "\n".join(out) + "\n"


def gen_call_args_6():
    return """\
// 6 arguments = exactly the SysV integer register set (RDI RSI RDX RCX
// R8 R9, ADR-010). Weights 1,2,4,8,16,32 make the result 63*1 unique
// for the identity mapping: any lost/swapped/perverted argument shows.
// The 4th argument is a CALL, so the caller must keep earlier register
// arguments intact across the nested call.
fn w6(a: i32, b: i32, c: i32, d: i32, e: i32, f: i32) -> i32 {
    return a + 2 * b + 4 * c + 8 * d + 16 * e + 32 * f;
}
fn ret2() -> i32 {
    print(100);
    return 2;
}
fn main() -> i32 {
    print(w6(1, 1, 1, 1, 1, 1));
    print(w6(63, 0, 0, 0, 0, 0));
    print(w6(0, 63, 0, 0, 0, 0));
    print(w6(1, 1, 1, ret2(), 1, 1));
    print(w6(0 - 1, 2, 0 - 3, 4, 0 - 5, 6));
    return 0;
}
"""


def gen_call_args_7():
    return """\
// 7 arguments = 6 registers + 1 stack argument (the odd stack-arg
// count: the caller pads the outgoing area to 16 bytes, ADR-010).
// Weights 1..64 make the identity mapping observable; the callee uses
// locals and a loop so its frame is non-trivial; argument 7 is a CALL
// so the stack slot must survive the nested call.
fn w7(a: i32, b: i32, c: i32, d: i32, e: i32, f: i32, g: i32) -> i32 {
    let mut acc: i32 = 0;
    let mut i: i32 = 0;
    while (i < 3) { acc = acc + i; i = i + 1; }
    return a + 2 * b + 4 * c + 8 * d + 16 * e + 32 * f + 64 * (g + acc);
}
fn ret1() -> i32 {
    print(200);
    return 1;
}
fn main() -> i32 {
    print(w7(1, 1, 1, 1, 1, 1, 1));
    print(w7(127, 0, 0, 0, 0, 0, 0));
    print(w7(0, 0, 0, 0, 0, 0, 127));
    print(w7(1, 2, 3, 4, 5, 6, ret1()));
    return 0;
}
"""


def gen_stack_arg_order():
    return """\
// 8 arguments = 6 registers + 2 stack arguments (KAVIR_MAX_ARGS, the
// ADR-010 capacity). Bits 1,2,4,...,128 encode each argument's
// POSITION into its VALUE: the weighted sum 1793 is reachable only by
// the identity mapping, so a wrong stack-arg order, a wrong slot
// stride, or a wrong register assignment all fail. Includes a nested
// w8 call in the LAST argument (stack args built while the caller's
// own outgoing area for the outer call is under construction) and a
// reversed-order call whose expected value differs.
fn w8(a: i32, b: i32, c: i32, d: i32, e: i32, f: i32, g: i32, h: i32) -> i32 {
    return a + 2 * b + 4 * c + 8 * d + 16 * e + 32 * f + 64 * g + 128 * h;
}
fn inner() -> i32 {
    return w8(1, 1, 1, 1, 1, 1, 1, 1);
}
fn main() -> i32 {
    print(w8(1, 1, 1, 1, 1, 1, 1, 1));
    print(w8(1, 2, 3, 4, 5, 6, 7, 8));
    print(w8(8, 7, 6, 5, 4, 3, 2, 1));
    print(w8(1, 1, 1, 1, 1, 1, 1, inner()));
    print(w8(0, 0, 0, 0, 0, 0, 255, 0));
    print(w8(0, 0, 0, 0, 0, 0, 0, 255));
    return 0;
}
"""


def main():
    os.makedirs(CASES, exist_ok=True)
    write("labels_50_fns_ok.k", gen_50_fns(None))
    write("labels_50_fns_trap.k", gen_50_fns("divzero"))
    write("labels_50_fns_trap_overflow.k", gen_50_fns("overflow"))
    write("depth_1025_clamp.k", gen_depth_1025())
    write("depth_stdout_order.k", gen_depth_stdout_order())
    write("depth_mutual_ok.k", gen_depth_mutual(True))
    write("depth_mutual_trap.k", gen_depth_mutual(False))
    write("call_args_6.k", gen_call_args_6())
    write("call_args_7.k", gen_call_args_7())
    write("stack_arg_order.k", gen_stack_arg_order())


if __name__ == "__main__":
    main()

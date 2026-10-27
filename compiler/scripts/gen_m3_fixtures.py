#!/usr/bin/env python3
"""scripts/gen_m3_fixtures.py -- writes the M3 conformance fixtures into
tests/cases_m3/ (three-way cases, trap cases, negative cases). Idempotent:
re-running overwrites the same deterministic content."""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "tests", "cases_m3")

FIB = """\
fn fib(n: i32) -> i32 {
    if (n < 2) { return n; }
    return fib(n - 1) + fib(n - 2);
}
fn main() -> i32 {
    print(fib(0));
    print(fib(1));
    print(fib(2));
    print(fib(10));
    print(fib(20));
    return 0;
}
"""

MUTUAL = """\
fn is_even(n: i32) -> i32 {
    if (n == 0) { return 1; }
    return is_odd(n - 1);
}
fn is_odd(n: i32) -> i32 {
    if (n == 0) { return 0; }
    return is_even(n - 1);
}
fn main() -> i32 {
    print(is_even(10));
    print(is_odd(10));
    print(is_even(7));
    return 0;
}
"""

ARGS8 = """\
fn add8(a: i32, b: i32, c: i32, d: i32, e: i32, f: i32, g: i32, h: i32) -> i32 {
    return a + b + c + d + e + f + g + h;
}
fn main() -> i32 {
    print(add8(1, 2, 3, 4, 5, 6, 7, 8));
    print(add8(0, 0, 0, 0, 0, 0, 0, 0));
    print(add8(-1, -2, -3, -4, -5, -6, -7, -8));
    return 0;
}
"""

ARGS0 = """\
fn five() -> i32 {
    return 5;
}
fn main() -> i32 {
    print(five());
    let x: i32 = five();
    print(x + five());
    return 0;
}
"""

ARG_ORDER = """\
// argument expressions evaluate left-to-right BEFORE the call
// (ADR-010 decision 1); prints make the order observable
fn first() -> i32 {
    print(1);
    return 10;
}
fn second() -> i32 {
    print(2);
    return 20;
}
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}
fn main() -> i32 {
    print(add(first(), second()));
    return 0;
}
"""

WHILE_COUNT = """\
fn main() -> i32 {
    let mut i: i32 = 0;
    let mut s: i32 = 0;
    while (i < 5) {
        s = s + i;
        print(i);
        i = i + 1;
    }
    print(s);
    return 0;
}
"""

WHILE_ZERO = """\
fn main() -> i32 {
    let mut i: i32 = 10;
    while (i < 5) {
        print(i);
        i = i + 1;
    }
    print(i);
    print("loop skipped");
    return 0;
}
"""

WHILE_NESTED = """\
fn main() -> i32 {
    let mut a: i32 = 0;
    while (a < 2) {
        let mut b: i32 = 0;
        while (b < 3) {
            print(a * 10 + b);
            b = b + 1;
        }
        a = a + 1;
    }
    return 0;
}
"""

WHILE_CALL_COND = """\
fn limit() -> i32 {
    return 3;
}
fn main() -> i32 {
    let mut i: i32 = 0;
    while (i < limit()) {
        print(i);
        i = i + 1;
    }
    return 0;
}
"""

LET_IMMUTABLE = """\
fn main() -> i32 {
    let x: i32 = 3;
    let y: i32 = x * 2 + 1;
    print(x);
    print(y);
    return y;
}
"""

LET_MUT = """\
fn main() -> i32 {
    let mut x: i32 = 1;
    print(x);
    x = x + 41;
    print(x);
    x = x * 2;
    print(x);
    return x;
}
"""

LET_IN_LOOP = """\
// a let inside a loop body re-executes (re-binds) on every iteration
fn main() -> i32 {
    let mut i: i32 = 0;
    while (i < 3) {
        let snap: i32 = i * 100;
        print(snap);
        i = i + 1;
    }
    return 0;
}
"""

CMP_MATRIX = """\
fn main() -> i32 {
    print(2 < 3);
    print(3 < 3);
    print(2 <= 3);
    print(3 <= 3);
    print(3 > 2);
    print(3 > 3);
    print(3 >= 3);
    print(2 >= 3);
    print(4 == 4);
    print(4 == 5);
    print(4 != 5);
    print(4 != 4);
    return 0;
}
"""

CMP_EDGE = """\
fn main() -> i32 {
    print(-2147483648 < 2147483647);
    print(-2147483648 == 2147483647);
    print(-2147483648 <= -2147483648);
    print(2147483647 >= 2147483647);
    print(2147483647 > -2147483648);
    print(0 == -0);
    return 0;
}
"""

EARLY_RETURN = """\
// 'return' is legal anywhere (ADR-010 decision 5)
fn classify(n: i32) -> i32 {
    if (n < 0) { return 0 - 1; }
    while (n > 100) {
        return 100;
    }
    if (n == 0) { return 1; }
    return 2;
}
fn main() -> i32 {
    print(classify(-5));
    print(classify(0));
    print(classify(9));
    print(classify(500));
    return 0;
}
"""

FALL_OFF = """\
// falling off the end returns 0 (both backends, documented)
fn noisy() -> i32 {
    print(777);
}
fn main() -> i32 {
    print(noisy());
    return 3;
}
"""

SUM_REC = """\
fn sum(n: i32) -> i32 {
    if (n == 0) { return 0; }
    return n + sum(n - 1);
}
fn main() -> i32 {
    print(sum(100));
    return 0;
}
"""

TRAP_IN_CALLEE = """\
// a trap inside a callee aborts with the callee's position; stdout
// written before the trap is kept
fn explode(n: i32) -> i32 {
    print(11);
    return n / (n - n);
}
fn main() -> i32 {
    print(explode(5));
    print(99);
    return 0;
}
"""

DEPTH_OK = """\
// 1024 simultaneous frames are legal: main + count(1022)..count(0)
fn count(n: i32) -> i32 {
    if (n == 0) { return 0; }
    return 1 + count(n - 1);
}
fn main() -> i32 {
    print(count(1022));
    return 0;
}
"""

DEPTH_TRAP = """\
// the 1025th simultaneous frame traps (ADR-010: limit 1024, user
// decision 2026-10-26); the trap names the callee and its definition
fn count(n: i32) -> i32 {
    if (n == 0) { return 0; }
    return 1 + count(n - 1);
}
fn main() -> i32 {
    print(count(1023));
    print(1);
    return 0;
}
"""

TRAP_IN_LOOP = """\
// iterations print before the trap fires mid-loop
fn main() -> i32 {
    let mut i: i32 = 0;
    while (i < 10) {
        print(i);
        i = i + 1;
    }
    print(100 / (5 - 5));
    return 0;
}
"""

TRAP_MID_ARGS = """\
// argument order is observable through a trap between the two prints
fn loud() -> i32 {
    print(7);
    return 0 / 0;
}
fn pair(a: i32, b: i32) -> i32 {
    return a * 1000 + b;
}
fn main() -> i32 {
    print(pair(loud(), 2));
    return 0;
}
"""

PARAM_USE = """\
fn cube(x: i32) -> i32 {
    return x * x * x;
}
fn main() -> i32 {
    print(cube(3));
    print(cube(-4));
    let r: i32 = cube(2) + cube(3);
    print(r);
    return 0;
}
"""

NEGATIVES = [
    ("err_assign_to_imm.k",
     "// expect-error: cannot assign to immutable variable",
     """fn main() -> i32 {
    let x: i32 = 1;
    x = 2;
    return x;
}
"""),
    ("err_assign_to_param.k",
     "// expect-error: cannot assign to parameter",
     """fn f(n: i32) -> i32 {
    n = 1;
    return n;
}
fn main() -> i32 {
    return f(0);
}
"""),
    ("err_redeclare.k",
     "// expect-error: duplicate variable",
     """fn main() -> i32 {
    let x: i32 = 1;
    let x: i32 = 2;
    return x;
}
"""),
    ("err_redeclare_mut.k",
     "// expect-error: duplicate variable",
     """fn main() -> i32 {
    let mut x: i32 = 1;
    let x: i32 = 2;
    return x;
}
"""),
    ("err_param_dup.k",
     "// expect-error: duplicate parameter",
     """fn f(a: i32, a: i32) -> i32 {
    return a;
}
fn main() -> i32 {
    return f(1, 2);
}
"""),
    ("err_undeclared_use.k",
     "// expect-error: undeclared variable",
     """fn main() -> i32 {
    print(y);
    return 0;
}
"""),
    ("err_use_before_let.k",
     "// expect-error: undeclared variable",
     """fn main() -> i32 {
    let x: i32 = x + 1;
    return x;
}
"""),
    ("err_call_undefined.k",
     "// expect-error: call to undefined function",
     """fn main() -> i32 {
    return g();
}
"""),
    ("err_arity.k",
     "// expect-error: expects 2 argument\\(s\\), got 1",
     """fn add(a: i32, b: i32) -> i32 {
    return a + b;
}
fn main() -> i32 {
    return add(1);
}
"""),
    ("err_too_many_args.k",
     "// expect-error: too many arguments",
     """fn f(a: i32, b: i32, c: i32, d: i32, e: i32, ff: i32, g: i32, h: i32) -> i32 {
    return a + b + c + d + e + ff + g + h;
}
fn main() -> i32 {
    return f(1, 2, 3, 4, 5, 6, 7, 8, 9);
}
"""),
    ("err_too_many_params.k",
     "// expect-error: too many parameters",
     """fn f(a: i32, b: i32, c: i32, d: i32, e: i32, f2: i32, g: i32, h: i32, i: i32) -> i32 {
    return a;
}
fn main() -> i32 {
    return f(1, 2, 3, 4, 5, 6, 7, 8, 9);
}
"""),
    ("err_chain_cmp.k",
     "// expect-error: non-associative",
     """fn main() -> i32 {
    print(1 < 2 < 3);
    return 0;
}
"""),
    ("err_call_statement.k",
     "// expect-error: call statements do not exist",
     """fn f(x: i32) -> i32 {
    return x;
}
fn main() -> i32 {
    f(1);
    return 0;
}
"""),
    ("err_bang.k",
     "// expect-error: unexpected character '!'",
     """fn main() -> i32 {
    print(!1);
    return 0;
}
"""),
    ("err_let_no_init.k",
     "// expect-error: '=' and an initializer",
     """fn main() -> i32 {
    let x: i32;
    return 0;
}
"""),
    ("err_let_bad_type.k",
     "// expect-error: M3 variables are 'i32' only",
     """fn main() -> i32 {
    let x: i64 = 1;
    return 0;
}
"""),
    ("err_param_type.k",
     "// expect-error: M3 parameter types are 'i32' only",
     """fn f(a: i64) -> i32 {
    return 0;
}
fn main() -> i32 {
    return f(1);
}
"""),
]


def main():
    os.makedirs(OUT, exist_ok=True)
    positives = {
        "fib_recursion.k": FIB,
        "mutual_recursion.k": MUTUAL,
        "call_args_8.k": ARGS8,
        "call_args_0.k": ARGS0,
        "arg_eval_order.k": ARG_ORDER,
        "while_count.k": WHILE_COUNT,
        "while_zero_iter.k": WHILE_ZERO,
        "while_nested.k": WHILE_NESTED,
        "while_call_cond.k": WHILE_CALL_COND,
        "let_immutable.k": LET_IMMUTABLE,
        "let_mut_assign.k": LET_MUT,
        "let_in_loop.k": LET_IN_LOOP,
        "cmp_matrix.k": CMP_MATRIX,
        "cmp_edge.k": CMP_EDGE,
        "early_return.k": EARLY_RETURN,
        "fall_off_zero.k": FALL_OFF,
        "sum_recursion.k": SUM_REC,
        "param_use.k": PARAM_USE,
        "trap_in_callee.k": TRAP_IN_CALLEE,
        "trap_depth_boundary_ok.k": DEPTH_OK,
        "trap_depth_limit.k": DEPTH_TRAP,
        "trap_in_loop.k": TRAP_IN_LOOP,
        "trap_mid_args.k": TRAP_MID_ARGS,
        "early_return_in_if.k": None,  # already created by hand earlier
    }
    n = 0
    for name, body in positives.items():
        if body is None:
            continue
        with open(os.path.join(OUT, name), "w", encoding="utf-8") as f:
            f.write(body)
        n += 1
    for name, header, body in NEGATIVES:
        with open(os.path.join(OUT, name), "w", encoding="utf-8") as f:
            f.write(header + "\n" + body)
        n += 1
    print("wrote %d fixtures into %s" % (n, os.path.normpath(OUT)))
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""One-off generator for the M2 conformance fixtures in
kavir/compiler/tests/cases_m2/ (run from the repo root or anywhere).

Each positive case is verified by the THREE-way runner (tests/run_m2.py):
ELF == interpreter == tests/reference.py independent expectation.
Negative cases carry their required diagnostic pattern on the first line.
"""
import os

OUT = "/home/z/my-project/kavir/compiler/tests/cases_m2"


def w(name, body):
    with open(os.path.join(OUT, name), "w", encoding="utf-8") as f:
        f.write(body)


os.makedirs(OUT, exist_ok=True)

# ------------------------------------------------------------- positives
w("print_str_hello.k", """\
// print a string literal, byte-exact
fn main() -> i32 {
    print("Hello, Kavir!\\n");
    return 0;
}
""")

w("print_str_utf8.k", """\
// UTF-8 bytes pass through unchanged (ADR-008 decision 4)
fn main() -> i32 {
    print("کاویر\\n");
    print("OK\\n");
    return 0;
}
""")

w("print_str_escapes.k", """\
// every supported escape
fn main() -> i32 {
    print("a\\tb\\\\c\\"d\\n");
    return 0;
}
""")

w("print_int_basic.k", """\
// decimal rendering: 0, small, negative; no padding, '-' for negatives
fn main() -> i32 {
    print(0);
    print(" ");
    print(7);
    print(" ");
    print(-7);
    print("\\n");
    return 0;
}
""")

w("print_int_bounds.k", """\
// the two i32 extremes, incl. INT32_MIN via the unary-minus rule
fn main() -> i32 {
    print(2147483647);
    print(" ");
    print(-2147483648);
    print("\\n");
    return 0;
}
""")

w("add_ok.k", """\
// addition at the boundary: stays in range -> value
fn main() -> i32 {
    print(2147483646 + 1);
    print(" ");
    print(-2147483647 + (-1));
    print(" ");
    print(0 + 0);
    print("\\n");
    return 0;
}
""")

w("sub_ok.k", """\
// subtraction at the boundary
fn main() -> i32 {
    print(-2147483648 - (-1));
    print(" ");
    print(2147483647 - 0);
    print(" ");
    print(5 - 7);
    print("\\n");
    return 0;
}
""")

w("mul_ok.k", """\
// multiplication at the boundary: 46340^2 is the largest square
fn main() -> i32 {
    print(46340 * 46340);
    print(" ");
    print(-2147483648 * 1);
    print(" ");
    print(123456 * 0);
    print(" ");
    print(-46340 * -46340);
    print("\\n");
    return 0;
}
""")

w("div_ok.k", """\
// truncated division: sign matrix + edge dividends
fn main() -> i32 {
    print(7 / 2);
    print(" ");
    print(-7 / 2);
    print(" ");
    print(7 / -2);
    print(" ");
    print(-7 / -2);
    print(" ");
    print(0 / 5);
    print(" ");
    print(-2147483648 / 1);
    print("\\n");
    return 0;
}
""")

w("rem_ok.k", """\
// remainder takes the dividend's sign (C semantics)
fn main() -> i32 {
    print(7 % 2);
    print(" ");
    print(-7 % 2);
    print(" ");
    print(7 % -2);
    print(" ");
    print(-7 % -2);
    print(" ");
    print(5 % 5);
    print(" ");
    print(0 % 3);
    print("\\n");
    return 0;
}
""")

w("expr_precedence.k", """\
// precedence and associativity (ADR-008 grammar)
fn main() -> i32 {
    print(2 + 3 * 4);
    print(" ");
    print((2 + 3) * 4);
    print(" ");
    print(100 / 10 / 5);
    print(" ");
    print(100 - 10 - 5);
    print(" ");
    print(-2 * 3);
    print(" ");
    print(2 * -3);
    print("\\n");
    return 0;
}
""")

w("deep_expr.k", """\
// longer nested expressions
fn main() -> i32 {
    print(1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10);
    print(" ");
    print((1 + 2) * (3 + 4) * (5 - 3));
    print(" ");
    print(1 + (2 * (3 + (4 / 2))) - 1);
    print("\\n");
    return 0;
}
""")

w("if_else_true.k", """\
// nonzero condition takes the then-branch
fn main() -> i32 {
    if (1) {
        print("then\\n");
    } else {
        print("else\\n");
    }
    if (2 - 1) {
        print("arith-nonzero\\n");
    }
    return 0;
}
""")

w("if_else_false.k", """\
// zero condition takes the else-branch
fn main() -> i32 {
    if (0) {
        print("then\\n");
    } else {
        print("else\\n");
    }
    if (1 - 1) {
        print("not taken\\n");
    }
    print("done\\n");
    return 0;
}
""")

w("if_nested.k", """\
// nested if/else in both branches
fn main() -> i32 {
    if (0) {
        print("outer-then\\n");
    } else {
        if (2 - 1) {
            print("inner-then\\n");
        } else {
            print("inner-else\\n");
        }
    }
    if (1) {
        if (0) {
            print("no\\n");
        }
        print("outer-then\\n");
    }
    return 0;
}
""")

w("if_no_else.k", """\
// false condition with no else: nothing printed from the if
fn main() -> i32 {
    if (0) {
        print("no\\n");
    }
    print("done\\n");
    return 0;
}
""")

w("if_trap_in_branch.k", """\
// sequential semantics: stdout written BEFORE the trap stays written
fn main() -> i32 {
    if (1) {
        print("t\\n");
        print(1 / 0);
    } else {
        print("e\\n");
    }
    return 0;
}
""")

w("exit_code_value.k", """\
// exit code = main's return value & 0xFF (300 & 0xFF = 44)
fn main() -> i32 {
    print("bye\\n");
    return 300;
}
""")

# ---------------------------------------------------------------- traps
w("trap_add_pos.k", """\
// INT32_MAX + 1 traps at the '+' (col of the operator)
fn main() -> i32 {
    print("a\\n");
    print(2147483647 + 1);
    print("unreachable\\n");
    return 0;
}
""")

w("trap_add_neg.k", """\
// INT32_MIN + -1 traps
fn main() -> i32 {
    print(-2147483648 + (-1));
    return 0;
}
""")

w("trap_sub_min.k", """\
// INT32_MIN - 1 traps
fn main() -> i32 {
    print(-2147483648 - 1);
    return 0;
}
""")

w("trap_sub_max.k", """\
// INT32_MAX - (-1) traps
fn main() -> i32 {
    print(2147483647 - (-1));
    return 0;
}
""")

w("trap_mul_46341.k", """\
// 46341^2 is the first square above i32
fn main() -> i32 {
    print(46341 * 46341);
    return 0;
}
""")

w("trap_mul_min_m1.k", """\
// INT32_MIN * -1 would be +2^31
fn main() -> i32 {
    print(-2147483648 * (-1));
    return 0;
}
""")

w("trap_mul_65536.k", """\
// 65536^2 = 2^32
fn main() -> i32 {
    print(65536 * 65536);
    return 0;
}
""")

w("trap_div_zero.k", """\
// division by zero traps (never a hardware SIGFPE)
fn main() -> i32 {
    print("x\\n");
    print(1 / 0);
    return 0;
}
""")

w("trap_div_min_m1.k", """\
// INT32_MIN / -1 traps (ADR-007 implementation note)
fn main() -> i32 {
    print(-2147483648 / -1);
    return 0;
}
""")

w("trap_rem_zero.k", """\
// remainder by zero traps with the division-by-zero message
fn main() -> i32 {
    print(7 % 0);
    return 0;
}
""")

w("trap_rem_min_m1.k", """\
// INT32_MIN % -1 traps (hardware-consistent; ADR-007)
fn main() -> i32 {
    print(-2147483648 % -1);
    return 0;
}
""")

w("trap_mid_expr.k", """\
// trap in the middle of an expression; nothing printed
fn main() -> i32 {
    print(10 + (2147483647 + 1));
    return 0;
}
""")

# ------------------------------------------------------------- negatives
w("err_escape.k", """\
// expect-error: unknown escape
fn main() -> i32 {
    print("a\\qb");
    return 0;
}
""")

w("err_unterminated.k", """\
// expect-error: unterminated string
fn main() -> i32 {
    print("abc
    return 0;
}
""")

w("err_neg_paren.k", """\
// expect-error: unary '-' in M2 applies to integer literals only
fn main() -> i32 {
    print(-(2147483648));
    return 0;
}
""")

w("err_double_neg.k", """\
// expect-error: unary '-' in M2 applies to integer literals only
fn main() -> i32 {
    print(--2147483648);
    return 0;
}
""")

w("err_unary_expr.k", """\
// expect-error: unary '-' in M2 applies to integer literals only
fn main() -> i32 {
    print(-(1 + 2));
    return 0;
}
""")

w("err_return_in_if.k", """\
// expect-error: 'return' inside an 'if' block
fn main() -> i32 {
    if (1) {
        return 1;
    }
    return 0;
}
""")

w("err_lit_in_expr.k", """\
// expect-error: does not fit in i32
fn main() -> i32 {
    print(2147483648 + 0);
    return 0;
}
""")

w("err_bare_2147483648.k", """\
// expect-error: does not fit in i32
fn main() -> i32 {
    print(2147483648);
    return 0;
}
""")

w("err_missing_semi.k", """\
// expect-error: expected ';'
fn main() -> i32 {
    print(1)
    return 0;
}
""")

w("err_else_alone.k", """\
// expect-error: unexpected 'else'
fn main() -> i32 {
    if (1) {
        print("a\\n");
    } else {
        print("b\\n");
    } else {
        print("c\\n");
    }
    return 0;
}
""")

print("wrote fixtures to", OUT)

// expect-error: pattern type 'A' does not match the scrutinee type 'B'
// M4.2 shape S1 (verification round 2, bug 2): wrong pattern type on
// the INNER match of a nested pair -- must be diagnosed against the
// INNER scrutinee, not the outer one.
enum A {
    A1(i32),
    A2(i32),
}

enum B {
    B1(i32),
    B2(i32),
}

fn f(a: A, b: B) -> i32 {
    return match a {
        A.A1(x) => match b {
            A.A2(y) => x + y,
            B.B2(y) => x - y,
        },
        A.A2(x) => x,
    };
}

fn main() -> i32 {
    return 0;
}

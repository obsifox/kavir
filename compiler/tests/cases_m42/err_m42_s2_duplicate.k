// expect-error: duplicate pattern for variant 'B1'
// M4.2 shape S2: duplicate variant arm inside a NESTED match.
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
            B.B1(y) => x + y,
            B.B1(y) => x + 2 * y,
            B.B2(y) => x - y,
        },
        A.A2(x) => x,
    };
}

fn main() -> i32 {
    return 0;
}

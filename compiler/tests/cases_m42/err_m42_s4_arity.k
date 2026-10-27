// expect-error: variant 'B2' carries 2 value\(s\); the pattern binds 3
// M4.2 shape S4: wrong payload arity on an arm of a NESTED match.
enum A {
    A1(i32),
    A2(i32),
}

enum B {
    B1(i32),
    B2(i32, i32),
}

fn f(a: A, b: B) -> i32 {
    return match a {
        A.A1(x) => match b {
            B.B1(y) => x + y,
            B.B2(p, q, r) => x + p + q,
        },
        A.A2(x) => x,
    };
}

fn main() -> i32 {
    return 0;
}

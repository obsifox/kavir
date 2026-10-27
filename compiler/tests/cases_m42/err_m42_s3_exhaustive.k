// expect-error: match is not exhaustive: variant 'B2'
// M4.2 shape S3: the NESTED match is missing a variant (the outer
// match is exhaustive; only the inner one must be rejected).
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
        },
        A.A2(x) => x,
    };
}

fn main() -> i32 {
    return 0;
}

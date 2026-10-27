// expect-error: '_' must be the last match arm
// M4.2 extra shape: wildcard in the middle of a NESTED match.
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
            _ => 0,
            B.B1(y) => x + y,
        },
        A.A2(x) => x,
    };
}

fn main() -> i32 {
    return 0;
}

// expect-error: match arm bodies must be i32
// M4.2 extra shape: a NESTED match's arm body has aggregate type.
enum A {
    A1(i32),
    A2(i32),
}

struct P {
    x: i32,
    y: i32,
}

fn f(a: A, p: P) -> i32 {
    return match a {
        A.A1(x) => match a {
            A.A1(y) => p,
            A.A2(y) => p,
        },
        A.A2(x) => x,
    };
}

fn main() -> i32 {
    return 0;
}

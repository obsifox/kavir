// expect-error: match requires an enum value
// M4.2 extra shape: match on a non-enum (i32), NESTED in an arm body.
enum A {
    A1(i32),
    A2(i32),
}

fn f(a: A) -> i32 {
    return match a {
        A.A1(x) => match x {
            A.A1(y) => y,
            _ => 0,
        },
        A.A2(x) => x,
    };
}

fn main() -> i32 {
    return 0;
}

// expect-error: Option.None pattern requires an Option scrutinee
// M4.2 extra shape: Option.None arm on a user-enum scrutinee, NESTED
// inside another match (the inner match's arm walk must reach the
// Option arm with the INNER scrutinee type).
enum A {
    A1(i32),
    A2(i32),
}

fn f(a: A) -> i32 {
    return match a {
        A.A1(x) => match a {
            A.A1(y) => y,
            Option.None => 0,
        },
        A.A2(x) => x,
    };
}

fn main() -> i32 {
    return 0;
}

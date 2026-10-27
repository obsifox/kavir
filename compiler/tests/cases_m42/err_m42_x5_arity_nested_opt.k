// expect-error: Option.Some carries 1 value\(s\); the pattern binds 2
// M4.2 extra shape: Option.Some arity error NESTED in a user-enum
// match arm (also pins the doubled-"the" message text as-is).
enum A {
    A1(i32),
    A2(i32),
}

fn f(a: A, o: Option[i32]) -> i32 {
    return match a {
        A.A1(x) => match o {
            Option.Some(u, v) => u + v,
            Option.None => 0,
        },
        A.A2(x) => x,
    };
}

fn main() -> i32 {
    return 0;
}

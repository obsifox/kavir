// M4.2 (verification round 2, bug 2): nested match on USER enums, two
// levels. The arm pool is shared program-wide; before the fix the
// outer match walked the INNER match's arms (base+count contiguity).
// All four outer/inner paths must evaluate independently.
enum A {
    A1(i32),
    A2(i32),
}

enum B {
    B1(i32),
    B2(i32),
}

fn pick(a: A, b: B) -> i32 {
    return match a {
        A.A1(x) => match b {
            B.B1(y) => x + y,
            B.B2(y) => x - y,
        },
        A.A2(x) => match b {
            B.B1(y) => x * y,
            B.B2(y) => x + 0 - y,
        },
    };
}

fn pick_wild(a: A, b: B) -> i32 {
    return match a {
        A.A1(x) => match b {
            B.B1(y) => x + 10 * y,
            _ => 0 - 7,
        },
        _ => 99,
    };
}

fn main() -> i32 {
    print(pick(A.A1(10), B.B1(3)));
    print(pick(A.A1(10), B.B2(3)));
    print(pick(A.A2(10), B.B1(3)));
    print(pick(A.A2(10), B.B2(3)));
    print(pick_wild(A.A1(10), B.B1(3)));
    print(pick_wild(A.A1(10), B.B2(3)));
    print(pick_wild(A.A2(10), B.B1(3)));
    return 0;
}

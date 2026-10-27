// M4.2 (verification round 2, bug 2): three-level nesting on user
// enums. Each level's own arms must stay distinct through the shared
// arm pool; the values pin every leaf path.
enum A {
    A1(i32),
    A2(i32),
}

enum B {
    B1(i32),
    B2(i32),
}

enum C {
    C1(i32),
    C2(i32),
}

fn deep(a: A, b: B, c: C) -> i32 {
    return match a {
        A.A1(x) => match b {
            B.B1(y) => match c {
                C.C1(z) => x + y + z,
                C.C2(z) => x + y - z,
            },
            B.B2(y) => match c {
                C.C1(z) => x - y + z,
                C.C2(z) => x - y - z,
            },
        },
        A.A2(x) => match b {
            B.B1(y) => match c {
                C.C1(z) => x * y + z,
                C.C2(z) => x * y - z,
            },
            B.B2(y) => match c {
                C.C1(z) => x + y * z,
                C.C2(z) => x - y * z,
            },
        },
    };
}

fn main() -> i32 {
    print(deep(A.A1(10), B.B1(3), C.C1(2)));
    print(deep(A.A1(10), B.B1(3), C.C2(2)));
    print(deep(A.A1(10), B.B2(3), C.C1(2)));
    print(deep(A.A1(10), B.B2(3), C.C2(2)));
    print(deep(A.A2(10), B.B1(3), C.C1(2)));
    print(deep(A.A2(10), B.B1(3), C.C2(2)));
    print(deep(A.A2(10), B.B2(3), C.C1(2)));
    print(deep(A.A2(10), B.B2(3), C.C2(2)));
    return 0;
}

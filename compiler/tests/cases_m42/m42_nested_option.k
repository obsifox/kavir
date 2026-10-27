// M4.2 (verification round 2, bug 2): Option matches nested inside
// user-enum match arms and a user-enum match nested inside an
// Option.Some arm body. Option arms live in the SAME shared pool, so
// this pins the chain walk against Option patterns too.
enum Shape {
    Dot,
    Circle(i32),
}

enum Kind {
    KNum(i32),
    KDot,
}

fn opt_in_enum(s: Shape, o: Option[i32]) -> i32 {
    return match s {
        Shape.Dot => match o {
            Option.None => 0 - 1,
            Option.Some(v) => v,
        },
        Shape.Circle(r) => match o {
            Option.None => r,
            Option.Some(v) => r + v,
        },
    };
}

fn enum_in_opt(k: Kind, o: Option[i32]) -> i32 {
    return match o {
        Option.None => match k {
            Kind.KNum(v) => v,
            Kind.KDot => 0,
        },
        Option.Some(w) => match k {
            Kind.KNum(v) => v + w,
            Kind.KDot => w,
        },
    };
}

fn main() -> i32 {
    print(opt_in_enum(Shape.Dot, Option.None));
    print(opt_in_enum(Shape.Dot, Option.Some(41)));
    print(opt_in_enum(Shape.Circle(5), Option.None));
    print(opt_in_enum(Shape.Circle(5), Option.Some(3)));
    print(enum_in_opt(Kind.KNum(7), Option.None));
    print(enum_in_opt(Kind.KDot, Option.None));
    print(enum_in_opt(Kind.KNum(7), Option.Some(2)));
    print(enum_in_opt(Kind.KDot, Option.Some(2)));
    return 0;
}

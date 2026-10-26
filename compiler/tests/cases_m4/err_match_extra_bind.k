// expect-error: carries 1 value\(s\); the pattern binds 2
enum Shape {
    Circle(i32),
}
fn f(s: Shape) -> i32 {
    return match s {
        Shape.Circle(a, b) => a + b,
    };
}
fn main() -> i32 { return 0; }

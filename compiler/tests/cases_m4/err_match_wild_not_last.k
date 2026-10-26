// expect-error: '_' must be the last match arm
enum Shape {
    Dot,
}
fn f(s: Shape) -> i32 {
    return match s {
        _ => 0,
        Shape.Dot => 1,
    };
}
fn main() -> i32 { return 0; }

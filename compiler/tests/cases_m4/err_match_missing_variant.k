// expect-error: match is not exhaustive: variant 'Rect' is not covered
enum Shape {
    Dot,
    Rect(i32, i32),
}
fn f(s: Shape) -> i32 {
    return match s {
        Shape.Dot => 0,
    };
}
fn main() -> i32 { return 0; }

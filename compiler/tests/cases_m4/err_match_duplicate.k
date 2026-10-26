// expect-error: duplicate pattern for variant 'Dot'
enum Shape {
    Dot,
}
fn f(s: Shape) -> i32 {
    return match s {
        Shape.Dot => 0,
        Shape.Dot => 1,
    };
}
fn main() -> i32 { return 0; }

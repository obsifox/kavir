// expect-error: variant 'Shape.Circle' carries 1 value\(s\), got 2
enum Shape {
    Circle(i32),
}
fn main() -> i32 {
    let s: Shape = Shape.Circle(1, 2);
    print(1);
    return 0;
}

// expect-error: carries a payload; call it as
enum Shape {
    Circle(i32),
}
fn main() -> i32 {
    let s: Shape = Shape.Circle;
    print(1);
    return 0;
}

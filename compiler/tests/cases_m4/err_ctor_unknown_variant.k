// expect-error: enum 'Shape' has no variant 'Round'
enum Shape {
    Dot,
}
fn main() -> i32 {
    let s: Shape = Shape.Round;
    print(1);
    return 0;
}

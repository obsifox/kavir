// expect-error: only structs have named fields
enum Shape {
    Dot,
}
fn main() -> i32 {
    let s: Shape = Shape.Dot;
    print(s.n);
    return 0;
}

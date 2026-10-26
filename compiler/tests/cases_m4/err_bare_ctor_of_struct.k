// expect-error: is a struct; construct it with a literal
struct Point {
    x: i32,
    y: i32,
}
fn main() -> i32 {
    let p: Point = Point.x;
    print(p.x);
    return 0;
}

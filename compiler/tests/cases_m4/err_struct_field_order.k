// expect-error: field 1 of 'Point' must be 'x'
struct Point {
    x: i32,
    y: i32,
}
fn main() -> i32 {
    let p: Point = Point { y: 1, x: 2 };
    print(p.x);
    return 0;
}

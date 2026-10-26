// expect-error: has 2 field\(s\), literal supplies 1
struct Point {
    x: i32,
    y: i32,
}
fn main() -> i32 {
    let p: Point = Point { x: 1 };
    print(p.x);
    return 0;
}

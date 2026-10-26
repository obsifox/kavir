// expect-error: cannot assign to immutable variable 'p'
struct Point {
    x: i32,
    y: i32,
}
fn main() -> i32 {
    let p: Point = Point { x: 1, y: 2 };
    p.x = 3;
    print(p.x);
    return 0;
}

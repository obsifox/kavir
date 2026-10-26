// expect-error: cannot assign to parameter 'p'
struct Point {
    x: i32,
    y: i32,
}
fn f(p: Point) -> i32 {
    p.x = 3;
    return p.x;
}
fn main() -> i32 { return 0; }

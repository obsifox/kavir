// expect-error: require i32 operands
struct Point {
    x: i32,
    y: i32,
}
fn main() -> i32 {
    let p: Point = Point { x: 1, y: 2 };
    let q: Point = Point { x: 3, y: 4 };
    let s: i32 = p + q;
    print(s);
    return 0;
}

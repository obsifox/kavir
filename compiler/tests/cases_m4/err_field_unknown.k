// expect-error: struct 'Point' has no field 'z'
struct Point {
    x: i32,
    y: i32,
}
fn main() -> i32 {
    let p: Point = Point { x: 1, y: 2 };
    print(p.z);
    return 0;
}

// expect-error: argument 1 of 'show' must be 'Point', got 'Tri'
struct Point {
    x: i32,
    y: i32,
}
struct Tri {
    a: i32,
    b: i32,
    c: i32,
}
fn show(p: Point) -> i32 { return p.x; }
fn main() -> i32 {
    print(show(Tri { a: 1, b: 2, c: 3 }));
    return 0;
}

// expect-error: Option.Some payload must be 'i32', got 'Point'
struct Point {
    x: i32,
    y: i32,
}
fn main() -> i32 {
    let o: Option[i32] = Option.Some(Point { x: 1, y: 2 });
    print(1);
    return 0;
}

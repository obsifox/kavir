// expect-error: expects 2 argument\(s\), got 1
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}
fn main() -> i32 {
    return add(1);
}

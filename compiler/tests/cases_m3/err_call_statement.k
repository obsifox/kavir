// expect-error: call statements do not exist
fn f(x: i32) -> i32 {
    return x;
}
fn main() -> i32 {
    f(1);
    return 0;
}

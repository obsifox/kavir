// expect-error: M3 parameter types are 'i32' only
fn f(a: i64) -> i32 {
    return 0;
}
fn main() -> i32 {
    return f(1);
}

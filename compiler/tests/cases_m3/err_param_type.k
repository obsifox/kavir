// expect-error: unknown type 'i64'
fn f(a: i64) -> i32 {
    return 0;
}
fn main() -> i32 {
    return f(1);
}

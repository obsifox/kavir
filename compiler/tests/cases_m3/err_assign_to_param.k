// expect-error: cannot assign to parameter
fn f(n: i32) -> i32 {
    n = 1;
    return n;
}
fn main() -> i32 {
    return f(0);
}

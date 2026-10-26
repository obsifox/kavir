// expect-error: cannot assign to immutable variable
fn main() -> i32 {
    let x: i32 = 1;
    x = 2;
    return x;
}

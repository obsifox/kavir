// expect-error: duplicate variable
fn main() -> i32 {
    let mut x: i32 = 1;
    let x: i32 = 2;
    return x;
}

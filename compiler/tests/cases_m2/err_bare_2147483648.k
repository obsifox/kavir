// expect-error: does not fit in i32
fn main() -> i32 {
    print(2147483648);
    return 0;
}

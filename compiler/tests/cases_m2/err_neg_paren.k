// expect-error: unary '-' in M2 applies to integer literals only
fn main() -> i32 {
    print(-(2147483648));
    return 0;
}

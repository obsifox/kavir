// expect-error: unary '-' in M2 applies to integer literals only
fn main() -> i32 {
    print(-(1 + 2));
    return 0;
}

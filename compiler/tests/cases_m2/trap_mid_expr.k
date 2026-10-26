// trap in the middle of an expression; nothing printed
fn main() -> i32 {
    print(10 + (2147483647 + 1));
    return 0;
}

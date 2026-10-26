// INT32_MIN + -1 traps
fn main() -> i32 {
    print(-2147483648 + (-1));
    return 0;
}

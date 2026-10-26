// INT32_MAX + 1 traps at the '+' (col of the operator)
fn main() -> i32 {
    print("a\n");
    print(2147483647 + 1);
    print("unreachable\n");
    return 0;
}

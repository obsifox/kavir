// addition at the boundary: stays in range -> value
fn main() -> i32 {
    print(2147483646 + 1);
    print(" ");
    print(-2147483647 + (-1));
    print(" ");
    print(0 + 0);
    print("\n");
    return 0;
}

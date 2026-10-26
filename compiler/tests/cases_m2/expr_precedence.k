// precedence and associativity (ADR-008 grammar)
fn main() -> i32 {
    print(2 + 3 * 4);
    print(" ");
    print((2 + 3) * 4);
    print(" ");
    print(100 / 10 / 5);
    print(" ");
    print(100 - 10 - 5);
    print(" ");
    print(-2 * 3);
    print(" ");
    print(2 * -3);
    print("\n");
    return 0;
}

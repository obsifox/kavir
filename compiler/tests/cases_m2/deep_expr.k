// longer nested expressions
fn main() -> i32 {
    print(1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10);
    print(" ");
    print((1 + 2) * (3 + 4) * (5 - 3));
    print(" ");
    print(1 + (2 * (3 + (4 / 2))) - 1);
    print("\n");
    return 0;
}

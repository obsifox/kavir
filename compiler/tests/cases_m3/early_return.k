// 'return' is legal anywhere (ADR-010 decision 5)
fn classify(n: i32) -> i32 {
    if (n < 0) { return 0 - 1; }
    while (n > 100) {
        return 100;
    }
    if (n == 0) { return 1; }
    return 2;
}
fn main() -> i32 {
    print(classify(-5));
    print(classify(0));
    print(classify(9));
    print(classify(500));
    return 0;
}

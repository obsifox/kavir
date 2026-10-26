fn is_even(n: i32) -> i32 {
    if (n == 0) { return 1; }
    return is_odd(n - 1);
}
fn is_odd(n: i32) -> i32 {
    if (n == 0) { return 0; }
    return is_even(n - 1);
}
fn main() -> i32 {
    print(is_even(10));
    print(is_odd(10));
    print(is_even(7));
    return 0;
}

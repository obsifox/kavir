fn fib(n: i32) -> i32 {
    if (n < 2) { return n; }
    return fib(n - 1) + fib(n - 2);
}
fn main() -> i32 {
    print(fib(0));
    print(fib(1));
    print(fib(2));
    print(fib(10));
    print(fib(20));
    return 0;
}

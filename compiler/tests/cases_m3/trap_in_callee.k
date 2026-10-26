// a trap inside a callee aborts with the callee's position; stdout
// written before the trap is kept
fn explode(n: i32) -> i32 {
    print(11);
    return n / (n - n);
}
fn main() -> i32 {
    print(explode(5));
    print(99);
    return 0;
}

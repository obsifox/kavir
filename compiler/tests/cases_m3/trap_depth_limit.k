// the 1025th simultaneous frame traps (ADR-010: limit 1024, user
// decision 2026-10-26); the trap names the callee and its definition
fn count(n: i32) -> i32 {
    if (n == 0) { return 0; }
    return 1 + count(n - 1);
}
fn main() -> i32 {
    print(count(1023));
    print(1);
    return 0;
}

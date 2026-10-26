// Boundary: first value above INT32_MAX -> compile-time error (ADR-007).
fn main() -> i32 {
    return 2147483648;
}

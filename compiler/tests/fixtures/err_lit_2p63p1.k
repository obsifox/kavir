// M1.1 regression: 2^63 + 1, same class as 2^63 (ADR-007).
fn main() -> i32 {
    return 9223372036854775809;
}

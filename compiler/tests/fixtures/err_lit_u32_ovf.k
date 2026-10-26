// Boundary: 2^32 -> compile-time error (ADR-007).
fn main() -> i32 {
    return 4294967296;
}

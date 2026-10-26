// Boundary: 2^64 exceeds u64 accumulation -> lexer "too large" (ADR-007).
fn main() -> i32 {
    return 18446744073709551616;
}

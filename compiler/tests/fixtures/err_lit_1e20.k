// Boundary: 10^20 exceeds u64 accumulation -> lexer "too large" (ADR-007).
fn main() -> i32 {
    return 99999999999999999999;
}

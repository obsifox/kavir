// Boundary: INT32_MAX itself MUST be accepted (ADR-007).
// Exit code 255 = 2147483647 & 0xFF (documented OS truncation).
fn main() -> i32 {
    return 2147483647;
}

// M1.1 regression: 2^63 used to wrap negative via signed overflow in the
// lexer and slip past the checker. MUST be rejected with the standard
// "does not fit in i32" diagnostic (ADR-007).
fn main() -> i32 {
    return 9223372036854775808;
}

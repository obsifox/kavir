// Boundary: INT64_MAX fits u64 accumulation but not i32 -> error (ADR-007).
fn main() -> i32 {
    return 9223372036854775807;
}

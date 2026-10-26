// depth 1025 requested is CLAMPED to the same observable trap: frames
// 0..1023 are legal (ADR-010 section 3b), entering frame 1024 traps,
// and no frame 1025 can ever exist. count(1024) would need 1026
// simultaneous frames; the trap bytes must equal the depth-1024 case
// (same message, same callee, exit 101).
fn count(n: i32) -> i32 {
    if (n == 0) { return 0; }
    return 1 + count(n - 1);
}
fn main() -> i32 {
    print(count(1024));
    return 0;
}

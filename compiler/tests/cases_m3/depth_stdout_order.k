// stdout BEFORE the depth trap: down(k) prints k on entry (frame k+1),
// then recurses. Entering frame 1024 traps in down's prologue BEFORE
// its print runs, so stdout must contain exactly 0..1022 in order
// (1023 lines) and NOTHING more; stderr is the frame-1024 trap line;
// exit 101. Pins stdout/stderr interleaving semantics of a trap.
fn down(n: i32) -> i32 {
    print(n);
    return down(n + 1);
}
fn main() -> i32 {
    print(0 - 1);
    return down(0);
}

// expect-error: 'return' inside an 'if' block
fn main() -> i32 {
    if (1) {
        return 1;
    }
    return 0;
}

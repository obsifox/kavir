// division by zero traps (never a hardware SIGFPE)
fn main() -> i32 {
    print("x\n");
    print(1 / 0);
    return 0;
}

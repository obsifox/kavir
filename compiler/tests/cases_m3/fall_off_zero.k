// falling off the end returns 0 (both backends, documented)
fn noisy() -> i32 {
    print(777);
}
fn main() -> i32 {
    print(noisy());
    return 3;
}

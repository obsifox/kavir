// 1024 simultaneous frames are legal: main + count(1022)..count(0)
fn count(n: i32) -> i32 {
    if (n == 0) { return 0; }
    return 1 + count(n - 1);
}
fn main() -> i32 {
    print(count(1022));
    return 0;
}

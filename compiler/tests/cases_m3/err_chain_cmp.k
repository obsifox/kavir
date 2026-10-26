// expect-error: non-associative
fn main() -> i32 {
    print(1 < 2 < 3);
    return 0;
}

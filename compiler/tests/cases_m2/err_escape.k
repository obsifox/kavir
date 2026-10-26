// expect-error: unknown escape
fn main() -> i32 {
    print("a\qb");
    return 0;
}

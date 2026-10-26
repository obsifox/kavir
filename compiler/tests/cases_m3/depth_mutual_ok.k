// mutual recursion at the depth boundary (ADR-010 3b):
// base at n==1022: deepest frame index 1023 (1024
// simultaneous frames) - exactly at the limit, legal.
fn down_a(n: i32) -> i32 {
    if (n == 1022) { return 0; }
    return down_b(n + 1);
}
fn down_b(n: i32) -> i32 {
    if (n == 1022) { return 0; }
    return down_a(n + 1);
}
fn main() -> i32 {
    print(down_a(0));
    return 0;
}

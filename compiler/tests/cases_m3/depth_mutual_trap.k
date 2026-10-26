// mutual recursion at the depth boundary (ADR-010 3b):
// base at n==1023: entering frame 1024 traps. Frame k
// runs down_a (k odd) or down_b (k even); frame 1024 is
// even, so the trap must name 'down_b' (verified three-way).
fn down_a(n: i32) -> i32 {
    if (n == 1023) { return 0; }
    return down_b(n + 1);
}
fn down_b(n: i32) -> i32 {
    if (n == 1023) { return 0; }
    return down_a(n + 1);
}
fn main() -> i32 {
    print(down_a(0));
    return 0;
}

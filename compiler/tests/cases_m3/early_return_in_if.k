// early 'return' inside 'if' became LEGAL in M3 (ADR-010 decision 5);
// this file was the M2 negative fixture err_return_in_if.k
fn pick(n: i32) -> i32 {
    if (n < 10) { return 1; }
    return 2;
}
fn main() -> i32 {
    print(pick(3));
    print(pick(30));
    return 0;
}

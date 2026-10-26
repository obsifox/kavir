// 8 arguments = 6 registers + 2 stack arguments (KAVIR_MAX_ARGS, the
// ADR-010 capacity). Bits 1,2,4,...,128 encode each argument's
// POSITION into its VALUE: the weighted sum 1793 is reachable only by
// the identity mapping, so a wrong stack-arg order, a wrong slot
// stride, or a wrong register assignment all fail. Includes a nested
// w8 call in the LAST argument (stack args built while the caller's
// own outgoing area for the outer call is under construction) and a
// reversed-order call whose expected value differs.
fn w8(a: i32, b: i32, c: i32, d: i32, e: i32, f: i32, g: i32, h: i32) -> i32 {
    return a + 2 * b + 4 * c + 8 * d + 16 * e + 32 * f + 64 * g + 128 * h;
}
fn inner() -> i32 {
    return w8(1, 1, 1, 1, 1, 1, 1, 1);
}
fn main() -> i32 {
    print(w8(1, 1, 1, 1, 1, 1, 1, 1));
    print(w8(1, 2, 3, 4, 5, 6, 7, 8));
    print(w8(8, 7, 6, 5, 4, 3, 2, 1));
    print(w8(1, 1, 1, 1, 1, 1, 1, inner()));
    print(w8(0, 0, 0, 0, 0, 0, 255, 0));
    print(w8(0, 0, 0, 0, 0, 0, 0, 255));
    return 0;
}

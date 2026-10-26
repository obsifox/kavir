// 6 arguments = exactly the SysV integer register set (RDI RSI RDX RCX
// R8 R9, ADR-010). Weights 1,2,4,8,16,32 make the result 63*1 unique
// for the identity mapping: any lost/swapped/perverted argument shows.
// The 4th argument is a CALL, so the caller must keep earlier register
// arguments intact across the nested call.
fn w6(a: i32, b: i32, c: i32, d: i32, e: i32, f: i32) -> i32 {
    return a + 2 * b + 4 * c + 8 * d + 16 * e + 32 * f;
}
fn ret2() -> i32 {
    print(100);
    return 2;
}
fn main() -> i32 {
    print(w6(1, 1, 1, 1, 1, 1));
    print(w6(63, 0, 0, 0, 0, 0));
    print(w6(0, 63, 0, 0, 0, 0));
    print(w6(1, 1, 1, ret2(), 1, 1));
    print(w6(0 - 1, 2, 0 - 3, 4, 0 - 5, 6));
    return 0;
}

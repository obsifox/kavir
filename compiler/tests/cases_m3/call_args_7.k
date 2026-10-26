// 7 arguments = 6 registers + 1 stack argument (the odd stack-arg
// count: the caller pads the outgoing area to 16 bytes, ADR-010).
// Weights 1..64 make the identity mapping observable; the callee uses
// locals and a loop so its frame is non-trivial; argument 7 is a CALL
// so the stack slot must survive the nested call.
fn w7(a: i32, b: i32, c: i32, d: i32, e: i32, f: i32, g: i32) -> i32 {
    let mut acc: i32 = 0;
    let mut i: i32 = 0;
    while (i < 3) { acc = acc + i; i = i + 1; }
    return a + 2 * b + 4 * c + 8 * d + 16 * e + 32 * f + 64 * (g + acc);
}
fn ret1() -> i32 {
    print(200);
    return 1;
}
fn main() -> i32 {
    print(w7(1, 1, 1, 1, 1, 1, 1));
    print(w7(127, 0, 0, 0, 0, 0, 0));
    print(w7(0, 0, 0, 0, 0, 0, 127));
    print(w7(1, 2, 3, 4, 5, 6, ret1()));
    return 0;
}

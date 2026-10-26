// M3.1 label-space stress: 50 functions, a branch and a
// division in every function (IR labels + ad-hoc div-skip
// labels + per-function depth stubs + arith stubs), three
// while loops, and trap sites in several different
// functions - ALL labels minted from the one backend
// allocator (M3.1 redesign). Verified three-way.
// Fires the division-by-zero site inside f37 AFTER the
// whole chain ran; stdout order pins what ran first.
fn f0(a: i32) -> i32 {
    if (a < 0) { return 0 - a / 2; }
    return f1(a - 1) + 1;
}
fn f1(a: i32) -> i32 {
    if (a < 1) { return 0 - a / 2; }
    return f2(a - 1) + 1;
}
fn f2(a: i32) -> i32 {
    if (a < 2) { return 0 - a / 2; }
    return f3(a - 1) + 1;
}
fn f3(a: i32) -> i32 {
    if (a < 3) { return 0 - a / 2; }
    return f4(a - 1) + 1;
}
fn f4(a: i32) -> i32 {
    if (a < 4) { return 0 - a / 2; }
    return f5(a - 1) + 1;
}
fn f5(a: i32) -> i32 {
    if (a < 5) { return 0 - a / 2; }
    return f6(a - 1) + 1;
}
fn f6(a: i32) -> i32 {
    if (a < 6) { return 0 - a / 2; }
    return f7(a - 1) + 1;
}
fn f7(a: i32) -> i32 {
    if (a < 7) { return 0 - a / 2; }
    return f8(a - 1) + 1;
}
fn f8(a: i32) -> i32 {
    if (a < 8) { return 0 - a / 2; }
    return f9(a - 1) + 1;
}
fn f9(a: i32) -> i32 {
    if (a < 9) { return 0 - a / 2; }
    return f10(a - 1) + 1;
}
fn f10(a: i32) -> i32 {
    if (a < 10) { return 0 - a / 2; }
    let mut x: i32 = a;
    while (2 < x) { x = x - 3; }
    return f11(a - 1) + x;
}
fn f11(a: i32) -> i32 {
    if (a < 11) { return 0 - a / 2; }
    return f12(a - 1) + 1;
}
fn f12(a: i32) -> i32 {
    if (a < 12) { return 0 - a / 2; }
    return f13(a - 1) + 1;
}
fn f13(a: i32) -> i32 {
    if (a < 13) { return 0 - a / 2; }
    return f14(a - 1) + 1;
}
fn f14(a: i32) -> i32 {
    if (a < 14) { return 0 - a / 2; }
    return f15(a - 1) + 1;
}
fn f15(a: i32) -> i32 {
    if (a < 15) { return 0 - a / 2; }
    return f16(a - 1) + 1;
}
fn f16(a: i32) -> i32 {
    if (a < 16) { return 0 - a / 2; }
    return f17(a - 1) + 1;
}
fn f17(a: i32) -> i32 {
    if (a < 17) { return 0 - a / 2; }
    return f18(a - 1) + 1;
}
fn f18(a: i32) -> i32 {
    if (a < 18) { return 0 - a / 2; }
    return f19(a - 1) + 1;
}
fn f19(a: i32) -> i32 {
    if (a < 19) { return 0 - a / 2; }
    return f20(a - 1) + 1;
}
fn f20(a: i32) -> i32 {
    if (a < 20) { return 0 - a / 2; }
    let mut x: i32 = a;
    while (2 < x) { x = x - 3; }
    return f21(a - 1) + x;
}
fn f21(a: i32) -> i32 {
    if (a < 21) { return 0 - a / 2; }
    return f22(a - 1) + 1;
}
fn f22(a: i32) -> i32 {
    if (a < 22) { return 0 - a / 2; }
    return f23(a - 1) + 1;
}
fn f23(a: i32) -> i32 {
    if (a < 23) { return 0 - a / 2; }
    return f24(a - 1) + 1;
}
fn f24(a: i32) -> i32 {
    if (a < 24) { return 0 - a / 2; }
    return f25(a - 1) + 1;
}
fn f25(a: i32) -> i32 {
    if (a < 25) { return 0 - a / 2; }
    return f26(a - 1) + 1;
}
fn f26(a: i32) -> i32 {
    if (a < 26) { return 0 - a / 2; }
    return f27(a - 1) + 1;
}
fn f27(a: i32) -> i32 {
    if (a < 27) { return 0 - a / 2; }
    return f28(a - 1) + 1;
}
fn f28(a: i32) -> i32 {
    if (a < 28) { return 0 - a / 2; }
    return f29(a - 1) + 1;
}
fn f29(a: i32) -> i32 {
    if (a < 29) { return 0 - a / 2; }
    return f30(a - 1) + 1;
}
fn f30(a: i32) -> i32 {
    if (a < 30) { return 0 - a / 2; }
    let mut x: i32 = a;
    while (2 < x) { x = x - 3; }
    return f31(a - 1) + x;
}
fn f31(a: i32) -> i32 {
    if (a < 31) { return 0 - a / 2; }
    return f32(a - 1) + 1;
}
fn f32(a: i32) -> i32 {
    if (a < 32) { return 0 - a / 2; }
    return f33(a - 1) + 1;
}
fn f33(a: i32) -> i32 {
    if (a < 33) { return 0 - a / 2; }
    return f34(a - 1) + 1;
}
fn f34(a: i32) -> i32 {
    if (a < 34) { return 0 - a / 2; }
    return f35(a - 1) + 1;
}
fn f35(a: i32) -> i32 {
    if (a < 35) { return 0 - a / 2; }
    return f36(a - 1) + 1;
}
fn f36(a: i32) -> i32 {
    if (a < 36) { return 0 - a / 2; }
    return f37(a - 1) + 1;
}
fn f37(a: i32) -> i32 {
    if (a < 0) { return 10 / (a + 1); }
    return f38(a - 1) + 1;
}
fn f38(a: i32) -> i32 {
    if (a < 38) { return 0 - a / 2; }
    return f39(a - 1) + 1;
}
fn f39(a: i32) -> i32 {
    if (a < 39) { return 0 - a / 2; }
    return f40(a - 1) + 1;
}
fn f40(a: i32) -> i32 {
    if (a < 40) { return 0 - a / 2; }
    return f41(a - 1) + 1;
}
fn f41(a: i32) -> i32 {
    if (a < 41) { return 0 - a / 2; }
    return f42(a - 1) + 1;
}
fn f42(a: i32) -> i32 {
    if (a < 42) { return 0 - a / 2; }
    return f43(a - 1) + 1;
}
fn f43(a: i32) -> i32 {
    if (a < 43) { return 0 - a / 2; }
    return f44(a - 1) + 1;
}
fn f44(a: i32) -> i32 {
    if (a < 44) { return 0 - a / 2; }
    return f45(a - 1) + 1;
}
fn f45(a: i32) -> i32 {
    if (a < 45) { return 0 - a / 2; }
    return f46(a - 1) + 1;
}
fn f46(a: i32) -> i32 {
    if (a < 46) { return 0 - a / 2; }
    return f47(a - 1) + 1;
}
fn f47(a: i32) -> i32 {
    if (a < 47) { return 0 - a / 2; }
    return f48(a - 1) + 1;
}
fn f48(a: i32) -> i32 {
    if (a < 48) { return 0 - a / 2; }
    return f49(a - 1) + 1;
}
fn f49(a: i32) -> i32 {
    if (a < 49) { return 0 - a / 2; }
    return a / 2;
}
fn main() -> i32 {
    print(f0(98));
    print(f37(0 - 1));
    print(7);
    return 0;
}

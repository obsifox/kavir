// nonzero condition takes the then-branch
fn main() -> i32 {
    if (1) {
        print("then\n");
    } else {
        print("else\n");
    }
    if (2 - 1) {
        print("arith-nonzero\n");
    }
    return 0;
}

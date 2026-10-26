// expect-error: unexpected 'else'
fn main() -> i32 {
    if (1) {
        print("a\n");
    } else {
        print("b\n");
    } else {
        print("c\n");
    }
    return 0;
}

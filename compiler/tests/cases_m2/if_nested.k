// nested if/else in both branches
fn main() -> i32 {
    if (0) {
        print("outer-then\n");
    } else {
        if (2 - 1) {
            print("inner-then\n");
        } else {
            print("inner-else\n");
        }
    }
    if (1) {
        if (0) {
            print("no\n");
        }
        print("outer-then\n");
    }
    return 0;
}

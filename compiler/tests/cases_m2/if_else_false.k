// zero condition takes the else-branch
fn main() -> i32 {
    if (0) {
        print("then\n");
    } else {
        print("else\n");
    }
    if (1 - 1) {
        print("not taken\n");
    }
    print("done\n");
    return 0;
}

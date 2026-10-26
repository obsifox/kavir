// false condition with no else: nothing printed from the if
fn main() -> i32 {
    if (0) {
        print("no\n");
    }
    print("done\n");
    return 0;
}

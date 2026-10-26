// sequential semantics: stdout written BEFORE the trap stays written
fn main() -> i32 {
    if (1) {
        print("t\n");
        print(1 / 0);
    } else {
        print("e\n");
    }
    return 0;
}

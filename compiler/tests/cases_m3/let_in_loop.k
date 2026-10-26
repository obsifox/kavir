// a let inside a loop body re-executes (re-binds) on every iteration
fn main() -> i32 {
    let mut i: i32 = 0;
    while (i < 3) {
        let snap: i32 = i * 100;
        print(snap);
        i = i + 1;
    }
    return 0;
}

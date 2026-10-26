// iterations print before the trap fires mid-loop
fn main() -> i32 {
    let mut i: i32 = 0;
    while (i < 10) {
        print(i);
        i = i + 1;
    }
    print(100 / (5 - 5));
    return 0;
}

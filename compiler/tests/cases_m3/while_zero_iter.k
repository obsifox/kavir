fn main() -> i32 {
    let mut i: i32 = 10;
    while (i < 5) {
        print(i);
        i = i + 1;
    }
    print(i);
    print("loop skipped");
    return 0;
}

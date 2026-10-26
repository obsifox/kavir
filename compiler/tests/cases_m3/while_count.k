fn main() -> i32 {
    let mut i: i32 = 0;
    let mut s: i32 = 0;
    while (i < 5) {
        s = s + i;
        print(i);
        i = i + 1;
    }
    print(s);
    return 0;
}

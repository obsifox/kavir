fn main() -> i32 {
    let mut a: i32 = 0;
    while (a < 2) {
        let mut b: i32 = 0;
        while (b < 3) {
            print(a * 10 + b);
            b = b + 1;
        }
        a = a + 1;
    }
    return 0;
}

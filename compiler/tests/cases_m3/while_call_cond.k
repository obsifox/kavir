fn limit() -> i32 {
    return 3;
}
fn main() -> i32 {
    let mut i: i32 = 0;
    while (i < limit()) {
        print(i);
        i = i + 1;
    }
    return 0;
}

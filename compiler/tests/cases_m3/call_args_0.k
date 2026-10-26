fn five() -> i32 {
    return 5;
}
fn main() -> i32 {
    print(five());
    let x: i32 = five();
    print(x + five());
    return 0;
}

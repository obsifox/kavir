// argument order is observable through a trap between the two prints
fn loud() -> i32 {
    print(7);
    return 0 / 0;
}
fn pair(a: i32, b: i32) -> i32 {
    return a * 1000 + b;
}
fn main() -> i32 {
    print(pair(loud(), 2));
    return 0;
}

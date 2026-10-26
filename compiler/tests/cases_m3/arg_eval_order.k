// argument expressions evaluate left-to-right BEFORE the call
// (ADR-010 decision 1); prints make the order observable
fn first() -> i32 {
    print(1);
    return 10;
}
fn second() -> i32 {
    print(2);
    return 20;
}
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}
fn main() -> i32 {
    print(add(first(), second()));
    return 0;
}

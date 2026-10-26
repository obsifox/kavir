fn cube(x: i32) -> i32 {
    return x * x * x;
}
fn main() -> i32 {
    print(cube(3));
    print(cube(-4));
    let r: i32 = cube(2) + cube(3);
    print(r);
    return 0;
}

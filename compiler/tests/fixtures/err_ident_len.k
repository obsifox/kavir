// M1.1 review item: identifiers longer than 63 characters must fail
// with a distinct diagnostic, never be silently truncated.
fn aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa() -> i32 {
    return 1;
}

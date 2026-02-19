// Test basic generic enum (Option<T>)
// Expected exit: 1

enum Option<T> {
    None,
    Some(T),
}

func main() -> i32 {
    var x: Option<i64> = Option::Some(42);
    var n: Option<i64> = Option::None;

    // Check tags
    if (x.tag != 1) { return 1; }
    if (n.tag != 0) { return 2; }

    return 0;
}

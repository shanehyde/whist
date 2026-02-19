// Expected error: expects 1 binding(s), got 2

enum Option<T> {
    Some(T),
    None,
}

func main() -> i32 {
    var opt: Option<i64> = Option::Some(42);
    match (opt) {
        Some(a, b) => { return 1; },
        None => { return 0; },
    }
    return 0;
}

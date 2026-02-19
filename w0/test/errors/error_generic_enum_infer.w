// Expected error: Cannot infer type parameter

enum Option<T> {
    None,
    Some(T),
}

func main() -> i32 {
    var x = Option::None;
    return 0;
}

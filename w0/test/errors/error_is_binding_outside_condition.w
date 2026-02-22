// Expected error: 'is' pattern with bindings only allowed in if/while conditions

enum Option<T> {
    Some(T),
    None,
}

func main() -> i32 {
    var opt: Option<i64> = Option::Some(42);
    var b: bool = opt is Some(v);
    return 0;
}

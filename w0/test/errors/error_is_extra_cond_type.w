// Expected error: 'is' pattern '&&' condition must be bool, got 'i64'

enum Option<T> {
    Some(T),
    None,
}

func main() -> i32 {
    var opt: Option<i64> = Option::Some(42);
    if (opt is Some(v) && v + 1) {
        return 0;
    }
    return 0;
}

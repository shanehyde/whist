// Test that local definitions shadow prelude types

enum Option<T> {
    Some(T),
    None,
}

enum Result<T, E> {
    Ok(T),
    Err(E),
}

trait Drop {
    func drop(): void;
}

func main(): i32 {
    var x: Option<i64> = Option::Some(42);
    var y: Result<i64, string> = Result::Ok(1);
    return 0;
}

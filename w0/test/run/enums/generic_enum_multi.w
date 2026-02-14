// Test generic enum with multiple type parameters

enum Result<T, E> {
    Ok(T),
    Err(E),
}

func main(): i32 {
    var ok: Result<i64, string> = Result::Ok(42);
    var err: Result<i64, string> = Result::Err("bad");

    if (ok.tag != 0) { return 1; }
    if (err.tag != 1) { return 2; }

    return 0;
}

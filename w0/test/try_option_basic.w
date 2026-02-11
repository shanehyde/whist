// Test ? operator on Option type

enum Option<T> {
    Some(T),
    None,
}

func find_value(key: i64): Option<i64> {
    if (key > 0) {
        return Option::Some(key * 10);
    }
    return Option::None;
}

func process(): Option<bool> {
    var val = find_value(5)?;
    return Option::Some(val > 0);
}

func main(): i32 {
    return 0;
}

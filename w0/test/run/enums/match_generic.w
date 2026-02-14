// Test match on generic enum (Option<i64>)

enum Option<T> {
    Some(T),
    None,
}

func main(): i32 {
    var opt: Option<i64> = Option::Some(42);

    var result: i64 = 0;
    match (opt) {
        Some(v) => {
            result = v;
        },
        None => {
            result = -1;
        },
    }

    if (result != 42) {
        return 1;
    }

    // Test None case
    var empty: Option<i64> = Option::None;
    match (empty) {
        Some(v) => { result = v; },
        None => { result = -1; },
    }

    if (result != -1) {
        return 2;
    }

    return 0;
}

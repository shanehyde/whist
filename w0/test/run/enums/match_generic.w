// Expected: PASS: match_generic
// Test match on generic enum (Option<i64>)

enum Option<T> {
    Some(T),
    None,
}

test "match_generic" {
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

    assert(result == 42);

    // Test None case
    var empty: Option<i64> = Option::None;
    match (empty) {
        Some(v) => { result = v; },
        None => { result = -1; },
    }

    assert(result == -1);
}

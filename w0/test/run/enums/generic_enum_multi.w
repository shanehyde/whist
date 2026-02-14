// Expected: PASS: generic_enum_multi
// Test generic enum with multiple type parameters

enum Result<T, E> {
    Ok(T),
    Err(E),
}

test "generic_enum_multi" {
    var ok: Result<i64, string> = Result::Ok(42);
    var err: Result<i64, string> = Result::Err("bad");

    assert(ok.tag == 0);
    assert(err.tag == 1);
}

// Expected: PASS: generic_enum_method_generic_some
// Expected: PASS: generic_enum_method_generic_none

enum Maybe<T> {
    Some(T),
    None,
}

func (const Maybe<T>) map<U>(f: func(T) -> U) -> Maybe<U> {
    match (self) {
        Some(v) => return Maybe::Some(f(v));
        None => return Maybe::None;
    }
}

func is_even(x: i64) -> bool {
    return x % 2 == 0;
}

test "generic_enum_method_generic_some" {
    var some: Maybe<i64> = Maybe::Some(4);
    var mapped: Maybe<bool> = some.map(is_even);

    match (mapped) {
        Some(v) => assert(v);
        None => assert(false);
    }
}

test "generic_enum_method_generic_none" {
    var none: Maybe<i64> = Maybe::None;
    var mapped: Maybe<bool> = none.map(is_even);

    match (mapped) {
        None => assert(true);
        Some(_) => assert(false);
    }
}

func main() -> i32 {
    return 0;
}

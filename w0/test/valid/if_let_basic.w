// Basic if-is with Option and Result

enum Option<T> {
    Some(T),
    None,
}

enum Result<T, E> {
    Ok(T),
    Err(E),
}

func test_option(opt: Option<i64>) -> i64 {
    if (opt is Some(v)) {
        return v;
    } else {
        return -1;
    }
}

func test_result(res: Result<i64, string>) -> i64 {
    if (res is Ok(v)) {
        return v;
    } else {
        return -1;
    }
}

// No bindings (simple variant)
func test_none_check(opt: Option<i64>) -> bool {
    if (opt is None) {
        return true;
    }
    return false;
}

// else-if-is chain
func test_chain(opt: Option<i64>, res: Result<i64, string>) -> i64 {
    if (opt is Some(v)) {
        return v;
    } else if (res is Ok(v)) {
        return v;
    } else {
        return -1;
    }
}

// else-if (regular) after if-is
func test_else_if(opt: Option<i64>, flag: bool) -> i64 {
    if (opt is Some(v)) {
        return v;
    } else if (flag) {
        return 0;
    } else {
        return -1;
    }
}

// Bare type check (no bindings on data variant)
func test_bare_check(opt: Option<i64>) -> bool {
    if (opt is Some) {
        return true;
    }
    return false;
}

// Extra condition with &&
func test_extra_cond(opt: Option<i64>) -> i64 {
    if (opt is Some(v) && v > 10) {
        return v;
    }
    return -1;
}

func main() -> i32 {
    return 0;
}

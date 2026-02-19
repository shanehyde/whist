// Basic if-let with Option and Result

enum Option<T> {
    Some(T),
    None,
}

enum Result<T, E> {
    Ok(T),
    Err(E),
}

func test_option(opt: Option<i64>) -> i64 {
    if let Some(v) = opt {
        return v;
    } else {
        return -1;
    }
}

func test_result(res: Result<i64, string>) -> i64 {
    if let Ok(v) = res {
        return v;
    } else {
        return -1;
    }
}

// No bindings (simple variant)
func test_none_check(opt: Option<i64>) -> bool {
    if let None = opt {
        return true;
    }
    return false;
}

// else-if-let chain
func test_chain(opt: Option<i64>, res: Result<i64, string>) -> i64 {
    if let Some(v) = opt {
        return v;
    } else if let Ok(v) = res {
        return v;
    } else {
        return -1;
    }
}

// else-if (regular) after if-let
func test_else_if(opt: Option<i64>, flag: bool) -> i64 {
    if let Some(v) = opt {
        return v;
    } else if (flag) {
        return 0;
    } else {
        return -1;
    }
}

func main() -> i32 {
    return 0;
}

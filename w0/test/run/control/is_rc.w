// Expected: PASS: is_rc_if_binding
// Expected: PASS: is_rc_while_binding
// Expected: PASS: is_rc_if_else
// Expected: PASS: is_rc_chained

// Helper: returns a Result with an RC string payload
func make_ok(s: string) -> Result<string, string> {
    return Result::Ok(s);
}

func make_err(s: string) -> Result<string, string> {
    return Result::Err(s);
}

func make_some(s: string) -> Option<string> {
    return Option::Some(s);
}

test "is_rc_if_binding" {
    // Function-returned enum with RC field — binding must stay alive
    if (make_ok("hello") is Ok(msg)) {
        assert(msg == "hello");
    } else {
        assert(false);
    }

    // Err variant with RC binding
    if (make_err("oops") is Err(e)) {
        assert(e == "oops");
    }

    // Option with RC binding
    if (make_some("world") is Some(s)) {
        assert(s == "world");
    }

    // Non-matching path — no binding, owned temp still cleaned up
    var matched = false;
    if (make_ok("test") is Err(e)) {
        matched = true;
    }
    assert(!matched);
}

test "is_rc_while_binding" {
    // While loop extracting RC string from function-returned enum
    var count = 0;
    var i = 0;
    var items = new Vec<string>{"a", "b", "c"};

    while (make_some(items[i]) is Some(s) && i < 3) {
        assert(s == items[i]);
        count += 1;
        i += 1;
        if (i >= 3) {
            break;
        }
    }
    assert(count == 3);
}

test "is_rc_if_else" {
    // Else branch with owned temp — temp must be cleaned up even on mismatch
    var result = "";
    if (make_ok("yes") is Err(e)) {
        result = e;
    } else {
        result = "no match";
    }
    assert(result == "no match");

    // Match path
    if (make_ok("found") is Ok(msg)) {
        result = msg;
    } else {
        result = "not found";
    }
    assert(result == "found");
}

test "is_rc_chained" {
    // Chained is with multiple RC bindings from function calls
    var value = "";
    if (make_ok("first") is Ok(a) && make_some("second") is Some(b)) {
        value = a + " " + b;
    }
    assert(value == "first second");

    // First fails — second should never evaluate
    value = "unchanged";
    if (make_err("fail") is Ok(a) && make_some("second") is Some(b)) {
        value = a + " " + b;
    }
    assert(value == "unchanged");
}

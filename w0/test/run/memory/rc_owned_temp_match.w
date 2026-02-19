// Expected: PASS: rc_owned_temp_match
// Test owned temps in match subject expressions are properly cleaned up

struct Wrapper {
    tag: i64,
}

func make_wrapper(t: i64) -> Wrapper {
    return new Wrapper { tag: t };
}

test "rc_owned_temp_match" {
    var result: i64 = 0;

    // The Wrapper returned by make_wrapper() is an owned temp used as the
    // match subject. The sub-expression temp gets cleaned up, and the
    // __matchN value is dec'd after the match arms.
    match (make_wrapper(2).tag) {
        1 => { result = 10; },
        2 => { result = 20; },
        _ => { result = 30; },
    }

    assert(result == 20);
}

// Expected: PASS: rc_owned_temp_return
// Test owned temps in return expressions are properly cleaned up

struct Box {
    value: i64,
}

func make_box(v: i64): Box {
    return new Box { value: v };
}

// Return an intermediate value derived from an owned temp.
// The Box is an owned temp; its .value is extracted, then the Box is dec'd.
func get_value(v: i64): i64 {
    return make_box(v).value;
}

test "rc_owned_temp_return" {
    var result = get_value(42);
    assert(result == 42);

    var result2 = get_value(100);
    assert(result2 == 100);
}

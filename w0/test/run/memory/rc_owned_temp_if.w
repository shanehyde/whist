// Expected: PASS: rc_owned_temp_if
// Expected: PASS: rc_owned_temp_if_else_if
// Test owned temps in if/else-if conditions are properly cleaned up

struct Counter {
    value: i64,
}

func make_counter(v: i64) -> Counter {
    return new Counter { value: v };
}

test "rc_owned_temp_if" {
    var result: i64 = 0;

    // The Counter returned by make_counter() is an owned temp in the condition.
    // It must be hoisted before the if, and dec'd after the if/else chain.
    if (make_counter(10).value > 5) {
        result = 1;
    } else {
        result = 2;
    }

    assert(result == 1);

    if (make_counter(3).value > 5) {
        result = 10;
    } else {
        result = 20;
    }

    assert(result == 20);
}

test "rc_owned_temp_if_else_if" {
    var result: i64 = 0;

    // Each else-if condition may have its own owned temp
    if (make_counter(1).value == 10) {
        result = 1;
    } else if (make_counter(2).value == 20) {
        result = 2;
    } else if (make_counter(3).value == 3) {
        result = 3;
    } else {
        result = 4;
    }

    assert(result == 3);
}

// Expected: PASS: rc_owned_temp_while
// Test owned temps in while conditions are properly cleaned up each iteration

struct Counter {
    value: i64,
}

func make_counter(v: i64) -> Counter {
    return new Counter { value: v };
}

test "rc_owned_temp_while" {
    // Each iteration calls make_counter(), producing an owned temp in the condition.
    // The temp must be cleaned up each iteration via the for(;;) transformation.
    var remaining: i64 = 3;
    var iterations: i64 = 0;

    while (make_counter(remaining).value > 0) {
        iterations = iterations + 1;
        remaining = remaining - 1;
    }

    // remaining starts at 3: values are 3, 2, 1, 0
    // Loop runs while value > 0: iterations with 3, 2, 1 pass; 0 fails
    assert(iterations == 3);
}

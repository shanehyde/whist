// Expected: PASS: rc_scope
// Test RC cleanup at nested scope boundaries

struct Counter {
    value: i64,
}

test "rc_scope" {
    var result: i64 = 0;
    {
        var c = new Counter { value: 10 };
        result = c.value;
        // c is decremented here at block exit
    }
    assert(result == 10);
}

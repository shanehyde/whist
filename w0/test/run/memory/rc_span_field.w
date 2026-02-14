// Expected: PASS: rc_span_field
// RC RUNTIME TEST: Span<T> fields are value types (no RC cleanup).

struct Buffer {
    data: Span<i64>,
    count: i64,
}

test "rc_span_field" {
    var arr: [3]i64;
    arr[0] = 1;
    arr[1] = 2;
    arr[2] = 3;

    var s: Span<i64> = arr[:];
    var buf = new Buffer { data: s, count: 3 };

    assert(buf.count == 3);
    assert(buf.data.count == 3);
    assert(buf.data[1] == 2);
}

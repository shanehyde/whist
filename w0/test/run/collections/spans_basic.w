// Expected: PASS: spans_basic

test "spans_basic" {
    var arr: [5]i64;
    arr[0] = 1;
    arr[1] = 2;
    arr[2] = 3;
    arr[3] = 4;
    arr[4] = 5;

    var s: Span<i64> = arr[:];
    assert(s.count == 5);
    assert(s[0] == 1);
    assert(s[4] == 5);

    var s2: Span<i64> = arr[1:4];
    assert(s2.count == 3);
    assert(s2[0] == 2);

    var s3: Span<i64> = s2[1:];
    assert(s3.count == 2);

    var empty: Span<i64> = arr[0:0];
    assert(empty.count == 0);
}

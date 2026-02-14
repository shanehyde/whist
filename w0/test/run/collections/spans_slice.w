// Expected: PASS: spans_slice
// Test slice syntax variants

test "spans_slice" {
    var arr: [10]i64;
    arr[0] = 10;
    arr[1] = 20;
    arr[2] = 30;
    arr[3] = 40;
    arr[4] = 50;
    arr[5] = 60;
    arr[6] = 70;
    arr[7] = 80;
    arr[8] = 90;
    arr[9] = 100;

    // Test [:] - full slice
    var full: Span<i64> = arr[:];
    assert(full.count == 10);
    assert(full[0] == 10);
    assert(full[9] == 100);

    // Test [start:end]
    var middle: Span<i64> = arr[3:7];
    assert(middle.count == 4);
    assert(middle[0] == 40);
    assert(middle[3] == 70);

    // Test [start:]
    var tail: Span<i64> = arr[7:];
    assert(tail.count == 3);
    assert(tail[0] == 80);
    assert(tail[2] == 100);

    // Test [:end]
    var head: Span<i64> = arr[:4];
    assert(head.count == 4);
    assert(head[0] == 10);
    assert(head[3] == 40);

    // Slice of a span
    var sub: Span<i64> = full[2:8];
    assert(sub.count == 6);
    assert(sub[0] == 30);

    var subsub: Span<i64> = sub[1:4];
    assert(subsub.count == 3);
    assert(subsub[0] == 40);
    assert(subsub[2] == 60);
}

// Expected: PASS: spans_basic
// Expected: PASS: spans_slice
// Expected: PASS: spans_with_generics

// Shared definitions

struct Box<T> {
    value: T,
}

func (Box<T>) get() -> T {
    return self.value;
}

// Tests

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

test "spans_with_generics" {
    // Test with i64
    var arr: [3]i64;
    arr[0] = 100;
    arr[1] = 200;
    arr[2] = 300;

    var s: Span<i64> = arr[:];
    assert(s.count == 3);
    assert(s[1] == 200);

    // Create a box with span value as element
    var box = new Box<i64> {value: s[0]};
    assert(box.get() == 100);

    // Slice from span
    var s2: Span<i64> = s[1:];
    assert(s2.count == 2);
    assert(s2[0] == 200);
}

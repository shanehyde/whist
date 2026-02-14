// Expected: PASS: spans_with_generics
// Test spans with generic structs

struct Box<T> {
    value: T,
}

func (Box<T>) get(): T {
    return self.value;
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

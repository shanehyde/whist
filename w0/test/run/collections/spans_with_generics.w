// Test spans with generic structs
struct Box<T> {
    value: T,
}

func (Box<T>) get(): T {
    return self.value;
}

func main(): i32 {
    // Test with i64
    var arr: [3]i64;
    arr[0] = 100;
    arr[1] = 200;
    arr[2] = 300;

    var s: Span<i64> = arr[:];
    if (s.count != 3) { return 1; }
    if (s[1] != 200) { return 2; }

    // Create a box with span value as element
    var box = new Box<i64> {value: s[0]};
    if (box.get() != 100) { return 3; }

    // Slice from span
    var s2: Span<i64> = s[1:];
    if (s2.count != 2) { return 4; }
    if (s2[0] != 200) { return 5; }

    return 0;
}

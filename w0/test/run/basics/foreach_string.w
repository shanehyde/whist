// Expected: PASS: foreach_string
// Test foreach over string (yields char)

test "foreach_string" {
    var s: string = "abc";
    var count: i64 = 0;

    foreach (const c in s) {
        if (count == 0) {
            assert(c == 'a');
        }
        if (count == 1) {
            assert(c == 'b');
        }
        if (count == 2) {
            assert(c == 'c');
        }
        count = count + 1;
    }

    assert(count == 3);

    // Test empty string
    var empty: string = "";
    var empty_count: i64 = 0;
    foreach (const c in empty) {
        empty_count = empty_count + 1;
    }
    assert(empty_count == 0);
}

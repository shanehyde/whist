// Test foreach over string (yields char)

func main(): i32 {
    var s: string = "abc";
    var count: i64 = 0;

    foreach (const c in s) {
        if (count == 0) {
            if (c != 'a') { return 1; }
        }
        if (count == 1) {
            if (c != 'b') { return 2; }
        }
        if (count == 2) {
            if (c != 'c') { return 3; }
        }
        count = count + 1;
    }

    if (count != 3) { return 4; }

    // Test empty string
    var empty: string = "";
    var empty_count: i64 = 0;
    foreach (const c in empty) {
        empty_count = empty_count + 1;
    }
    if (empty_count != 0) { return 5; }

    return 0;
}

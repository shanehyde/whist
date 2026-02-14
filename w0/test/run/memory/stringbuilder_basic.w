// RC RUNTIME TEST: StringBuilder basic operations

import std;

func main(): i32 {
    var sb = new StringBuilder{};

    // Test append
    sb.append("hello");
    sb.append_char(' ');
    sb.append("world");

    // Verify len
    if (sb.len() != 11) {
        return 1;
    }

    // Verify capacity >= len
    if (sb.capacity() < 11) {
        return 2;
    }

    // Verify to_string
    var s: string = sb.to_string();
    if (s != "hello world") {
        return 3;
    }

    // Test append_line (appends string + newline)
    sb.clear();
    if (sb.len() != 0) {
        return 4;
    }

    sb.append_line("line1");
    sb.append("line2");
    var s2: string = sb.to_string();
    if (s2 != "line1\nline2") {
        return 5;
    }

    // Test clear preserves capacity
    var cap_before: i64 = sb.capacity();
    sb.clear();
    if (sb.len() != 0) {
        return 6;
    }
    if (sb.capacity() != cap_before) {
        return 7;
    }

    // Test to_string on empty builder
    var empty: string = sb.to_string();
    if (empty != "") {
        return 8;
    }

    return 0;
}

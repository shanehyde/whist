// Expected: PASS: stringbuilder_basic
// RC RUNTIME TEST: StringBuilder basic operations

test "stringbuilder_basic" {
    var sb = new StringBuilder{};

    // Test append
    sb.append("hello");
    sb.append_char(' ');
    sb.append("world");

    // Verify len
    assert(sb.len() == 11);

    // Verify capacity >= len
    assert(sb.capacity() >= 11);

    // Verify to_string
    var s: string = sb.to_string();
    assert(s == "hello world");

    // Test append_line (appends string + newline)
    sb.clear();
    assert(sb.len() == 0);

    sb.append_line("line1");
    sb.append("line2");
    var s2: string = sb.to_string();
    assert(s2 == "line1\nline2");

    // Test clear preserves capacity
    var cap_before: i64 = sb.capacity();
    sb.clear();
    assert(sb.len() == 0);
    assert(sb.capacity() == cap_before);

    // Test to_string on empty builder
    var empty: string = sb.to_string();
    assert(empty == "");
}

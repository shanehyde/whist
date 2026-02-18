// Expected: PASS: trim
// Expected: PASS: trim_start
// Expected: PASS: trim_end
// Expected: PASS: strip_prefix
// Expected: PASS: strip_suffix
// Expected: PASS: pad_left
// Expected: PASS: pad_right
// Expected: PASS: trim_rc_safety

test "trim" {
    assert("  hello  ".trim() == "hello");
    assert("  hello".trim() == "hello");
    assert("hello  ".trim() == "hello");
    assert("hello".trim() == "hello");
    assert("".trim() == "");
    assert("   ".trim() == "");
    assert(" \t\n\r hello \t\n\r ".trim() == "hello");
}

test "trim_start" {
    assert("  hello  ".trim_start() == "hello  ");
    assert("  hello".trim_start() == "hello");
    assert("hello  ".trim_start() == "hello  ");
    assert("".trim_start() == "");
    assert("   ".trim_start() == "");
    assert("\t\nhello".trim_start() == "hello");
}

test "trim_end" {
    assert("  hello  ".trim_end() == "  hello");
    assert("hello  ".trim_end() == "hello");
    assert("  hello".trim_end() == "  hello");
    assert("".trim_end() == "");
    assert("   ".trim_end() == "");
    assert("hello\t\n".trim_end() == "hello");
}

test "strip_prefix" {
    assert("hello world".strip_prefix("hello ") == "world");
    assert("hello world".strip_prefix("xyz") == "hello world");
    assert("hello world".strip_prefix("") == "hello world");
    assert("".strip_prefix("abc") == "");
    assert("aaa".strip_prefix("a") == "aa");
}

test "strip_suffix" {
    assert("hello world".strip_suffix(" world") == "hello");
    assert("hello world".strip_suffix("xyz") == "hello world");
    assert("hello world".strip_suffix("") == "hello world");
    assert("".strip_suffix("abc") == "");
    assert("aaa".strip_suffix("a") == "aa");
}

test "pad_left" {
    assert("hi".pad_left(5, ' ') == "   hi");
    assert("hi".pad_left(5, '0') == "000hi");
    assert("hello".pad_left(3, ' ') == "hello");
    assert("hello".pad_left(5, ' ') == "hello");
    assert("".pad_left(3, 'x') == "xxx");
}

test "pad_right" {
    assert("hi".pad_right(5, ' ') == "hi   ");
    assert("hi".pad_right(5, '0') == "hi000");
    assert("hello".pad_right(3, ' ') == "hello");
    assert("hello".pad_right(5, ' ') == "hello");
    assert("".pad_right(3, 'x') == "xxx");
}

test "trim_rc_safety" {
    // Use dynamic strings (concatenated) to ensure RC-allocated strings work
    var s = "  " + "hello" + "  ";
    var trimmed = s.trim();
    assert(trimmed == "hello");

    var prefix = "hello " + "world";
    var stripped = prefix.strip_prefix("hello ");
    assert(stripped == "world");

    var padded = ("hi" + "").pad_left(6, '-');
    assert(padded == "----hi");
}

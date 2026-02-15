// Expected: PASS: string_concat
// Expected: PASS: string_equality
// Expected: PASS: string_index
// Expected: PASS: string_length
// Expected: PASS: string_search
// Expected: PASS: string_slice
// Expected: PASS: string_ordering

test "string_concat" {
    var s = "hello" + " " + "world";
    assert(s == "hello world");
    var a = "foo";
    var b = "bar";
    assert(a + b == "foobar");
    assert("" + "x" == "x");
}

test "string_equality" {
    var s = "foo";
    assert(s == "foo");
    assert(s != "bar");
    assert("hello" == "hello");
    assert("hello" != "world");
}

test "string_index" {
    var s = "abc";
    var c = s[1];
    assert(c == 'b');
    assert("hello"[0] == 'h');
    assert("hello"[4] == 'o');
}

test "string_length" {
    var s = "hello";
    assert(s.length() == 5);
    assert("".length() == 0);
    assert("abc".length() == 3);
}

test "string_search" {
    var s = "hello world";
    assert(s.starts_with("hello"));
    assert(!s.starts_with("world"));
    assert(s.ends_with("world"));
    assert(!s.ends_with("hello"));
    assert(s.contains("llo w"));
    assert(!s.contains("xyz"));
    assert("".starts_with(""));
}

test "string_slice" {
    var s = "hello";
    assert(s[1:4] == "ell");
    assert(s[0:5] == "hello");
    assert(s[1:] == "ello");
    assert(s[:3] == "hel");
    assert(s[:] == "hello");
    assert("abcdef"[2:5] == "cde");
}

test "string_ordering" {
    assert("abc" < "abd");
    assert("abc" < "abcd");
    assert("b" > "a");
    assert("abc" <= "abc");
    assert("abc" >= "abc");
    assert(!("abc" > "abd"));
    assert("" < "a");
    assert(!("a" < "a"));
    assert("apple" < "banana");
}

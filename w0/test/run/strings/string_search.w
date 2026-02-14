// Expected: PASS: string_search
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

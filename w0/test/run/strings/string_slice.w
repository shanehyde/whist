// Expected: PASS: string_slice
test "string_slice" {
    var s = "hello";
    assert(s[1:4] == "ell");
    assert(s[0:5] == "hello");
    assert(s[1:] == "ello");
    assert(s[:3] == "hel");
    assert(s[:] == "hello");
    assert("abcdef"[2:5] == "cde");
}

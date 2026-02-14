// Expected: PASS: string_length
test "string_length" {
    var s = "hello";
    assert(s.length() == 5);
    assert("".length() == 0);
    assert("abc".length() == 3);
}

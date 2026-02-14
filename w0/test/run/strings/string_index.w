// Expected: PASS: string_index
test "string_index" {
    var s = "abc";
    var c = s[1];
    assert(c == 'b');
    assert("hello"[0] == 'h');
    assert("hello"[4] == 'o');
}

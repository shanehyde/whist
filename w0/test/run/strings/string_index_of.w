// Expected: PASS: string_index_of
test "string_index_of" {
    assert("hello world".index_of("world") == 6);
    assert("hello world".index_of("hello") == 0);
    assert("hello world".index_of("xyz") == -1);
    assert("abcabc".index_of("bc") == 1);
    assert("hello".index_of("") == 0);
}

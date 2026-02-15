// Expected: PASS: string_ordering
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

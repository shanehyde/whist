// Expected: PASS: string_equality
test "string_equality" {
    var s = "foo";
    assert(s == "foo");
    assert(s != "bar");
    assert("hello" == "hello");
    assert("hello" != "world");
}

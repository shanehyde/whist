// Expected: PASS: string_concat
test "string_concat" {
    var s = "hello" + " " + "world";
    assert(s == "hello world");
    var a = "foo";
    var b = "bar";
    assert(a + b == "foobar");
    assert("" + "x" == "x");
}

func main(): i32 {
    var s = "hello" + " " + "world";
    if (s != "hello world") { return 1; }
    var a = "foo";
    var b = "bar";
    if (a + b != "foobar") { return 2; }
    if ("" + "x" != "x") { return 3; }
    return 0;
}

func main(): i32 {
    var s = "foo";
    if (s != "foo") { return 1; }
    if (s == "bar") { return 2; }
    if ("hello" != "hello") { return 3; }
    if ("hello" == "world") { return 4; }
    return 0;
}

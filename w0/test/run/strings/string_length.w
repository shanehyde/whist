func main(): i32 {
    var s = "hello";
    if (s.length() != 5) { return 1; }
    if ("".length() != 0) { return 2; }
    if ("abc".length() != 3) { return 3; }
    return 0;
}

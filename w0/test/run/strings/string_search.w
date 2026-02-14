func main(): i32 {
    var s = "hello world";
    if (!s.starts_with("hello")) { return 1; }
    if (s.starts_with("world")) { return 2; }
    if (!s.ends_with("world")) { return 3; }
    if (s.ends_with("hello")) { return 4; }
    if (!s.contains("llo w")) { return 5; }
    if (s.contains("xyz")) { return 6; }
    if (!"".starts_with("")) { return 7; }
    return 0;
}

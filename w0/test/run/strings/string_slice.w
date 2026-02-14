func main(): i32 {
    var s = "hello";
    if (s[1:4] != "ell") { return 1; }
    if (s[0:5] != "hello") { return 2; }
    if (s[1:] != "ello") { return 3; }
    if (s[:3] != "hel") { return 4; }
    if (s[:] != "hello") { return 5; }
    if ("abcdef"[2:5] != "cde") { return 6; }
    return 0;
}

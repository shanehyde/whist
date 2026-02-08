func main(): i32 {
    var s = "abc";
    var c = s[1];
    if (c != 'b') { return 1; }
    if ("hello"[0] != 'h') { return 2; }
    if ("hello"[4] != 'o') { return 3; }
    return 0;
}

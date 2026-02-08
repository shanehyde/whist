import std;

func main(): i32 {
    if (std.parse_i64("42") != 42) { return 1; }
    if (std.parse_i64("-7") != -7) { return 2; }
    if (std.parse_i64("0") != 0) { return 3; }
    if (std.to_string(42) != "42") { return 4; }
    if (std.to_string(-7) != "-7") { return 5; }
    if (std.to_string(0) != "0") { return 6; }
    return 0;
}

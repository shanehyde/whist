// Expected: PASS: string_conversion
import std;

test "string_conversion" {
    assert(std.parse_i64("42") == 42);
    assert(std.parse_i64("-7") == -7);
    assert(std.parse_i64("0") == 0);
    assert(std.to_string(42) == "42");
    assert(std.to_string(-7) == "-7");
    assert(std.to_string(0) == "0");
}

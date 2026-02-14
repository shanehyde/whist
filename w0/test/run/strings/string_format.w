// Expected: PASS: string_format
import std;

test "string_format" {
    var s = std.format("x=%d", 42);
    assert(s == "x=42");
    var s2 = std.format("%s has %d items", "list", 3);
    assert(s2 == "list has 3 items");
    var s3 = std.format("hello");
    assert(s3 == "hello");
}

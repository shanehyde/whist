import std;

func main(): i32 {
    var s = std.format("x=%d", 42);
    if (s != "x=42") { return 1; }
    var s2 = std.format("%s has %d items", "list", 3);
    if (s2 != "list has 3 items") { return 2; }
    var s3 = std.format("hello");
    if (s3 != "hello") { return 3; }
    return 0;
}

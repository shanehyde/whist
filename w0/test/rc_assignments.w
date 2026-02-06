import std;
// Test RC copy semantics (shared reference)

struct Point {
    x: i64,
    y: i64,
}

func main(): i32 {
    var p = new Point { x: 10, y: 20 };
    var q = p;
    // Both p and q point to the same reference
    p = null; //new Point { x: 30, y: 40 };
    q = null;

    std.print("p: ({}, {}), q: {}\n");

    return 0;
}

// Test RC copy semantics (shared reference)

struct Point {
    x: i64,
    y: i64,
}

func main(): i32 {
    var p = new Point { x: 10, y: 20 };
    var q = p;
    // Both p and q point to the same allocation
    q.x = 42;
    if (p.x != 42) {
        return 1;
    }
    return 0;
}

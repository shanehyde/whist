// Expected: PASS: rc_copy
// Test RC copy semantics (shared reference)

struct Point {
    x: i64,
    y: i64,
}

test "rc_copy" {
    var p = new Point { x: 10, y: 20 };
    var q = p;
    // Both p and q point to the same allocation
    q.x = 42;
    assert(p.x == 42);
}

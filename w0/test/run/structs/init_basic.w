// Expected: PASS: init_basic

struct Point {
    x: i64,
    y: i64,
}

impl Point {
    func init(x: i64, y: i64) {
        self.x = x;
        self.y = y;
    }
}

test "init_basic" {
    var p = new Point(10, 20);
    assert(p.x == 10);
    assert(p.y == 20);
}

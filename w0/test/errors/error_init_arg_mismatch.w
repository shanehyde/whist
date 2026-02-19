// Expected error: init expects 2 argument(s), got 1

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

func main() -> i32 {
    var p = new Point(1);
    return 0;
}

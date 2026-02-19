// Expected error: has an init method; use 'new Point(args)'

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
    var p = new Point {x: 1, y: 2};
    return 0;
}

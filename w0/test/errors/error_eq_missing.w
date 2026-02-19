// Expected error: Cannot compare struct 'Point' with == (no Eq impl)
struct Point {
    x: i64,
    y: i64,
}

func main() -> i32 {
    var a = new Point { x: 1, y: 2 };
    var b = new Point { x: 1, y: 2 };
    if (a == b) {}
    return 0;
}

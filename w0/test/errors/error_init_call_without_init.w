// Expected error: does not have an init method

struct Point {
    x: i64,
    y: i64,
}

func main(): i32 {
    var p = new Point(1, 2);
    return 0;
}

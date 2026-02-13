// Expected error: Cannot compare enum 'Wrapper': variant 'Val' contains struct 'Point' without Eq impl
struct Point {
    x: i64,
    y: i64,
}

enum Wrapper {
    Val(Point),
    Empty,
}

func main(): i32 {
    var a = Wrapper::Val(new Point { x: 1, y: 2 });
    var b = Wrapper::Val(new Point { x: 1, y: 2 });
    if (a == b) {}
    return 0;
}

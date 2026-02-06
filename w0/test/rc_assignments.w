import std;
// Test RC copy semantics (shared reference)

struct Point {
    x: i64,
    y: i64,
}

struct Line {
    start: Point,
    end: Point,
}


func main(): i32 {
    var p = new Point { x: 10, y: 20 };
    var q = new Point { x: 30, y: 40 };
    var z = new Point { x: 50, y: 60 };

    var line1 = new Line { start: p, end: q };

    line1.start = z;

    return 0;
}

// ERROR TEST: contains requires element type supporting ==
// Expected error: Vec.contains requires element type supporting ==, got 'Point'

struct Point {
    x: i64,
}

func main() -> i32 {
    var points = new Vec<Point>{};
    var p = new Point{x: 1};
    var found = points.contains(p);
    return 0;
}

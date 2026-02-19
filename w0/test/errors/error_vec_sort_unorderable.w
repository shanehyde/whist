// ERROR TEST: sort requires orderable primitive element type
// Expected error: Vec.sort requires orderable primitive element type, got 'Point'

struct Point {
    x: i64,
}

func main() -> i32 {
    var points = new Vec<Point>{};
    points.sort();
    return 0;
}

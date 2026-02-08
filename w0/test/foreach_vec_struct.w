// Test foreach over Vec<Point> with struct field access

struct Point { x: i64, y: i64 }

func main(): i32 {
    var points = new Vec<Point>{};

    points.push(new Point{x: 1, y: 2});
    points.push(new Point{x: 3, y: 4});
    points.push(new Point{x: 5, y: 6});

    var sum_x: i64 = 0;
    var sum_y: i64 = 0;
    foreach (const p in points) {
        sum_x = sum_x + p.x;
        sum_y = sum_y + p.y;
    }

    if (sum_x != 9) {
        return 1;
    }

    if (sum_y != 12) {
        return 2;
    }

    return 0;
}

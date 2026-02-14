trait Clonable {
    func clone(): Self;
}

struct Point { x: i64, y: i64 }

impl Clonable for Point {
    func clone(): Point {
        return new Point { x: 10, y: 20 };
    }
}

func main(): i32 {
    var p = new Point { x: 1, y: 2 };
    var q = p.clone();
    return 0;
}

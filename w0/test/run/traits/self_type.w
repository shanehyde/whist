// Expected: PASS: self_type
// Expected: PASS: self_type_param

trait Clonable {
    func clone() -> Self;
}

struct Point { x: i64, y: i64 }

impl Clonable for Point {
    func clone() -> Point {
        return new Point { x: 10, y: 20 };
    }
}

trait Combinable {
    func combine(other: Self) -> Self;
}

struct Counter { value: i64 }

impl Combinable for Counter {
    func combine(other: Counter) -> Counter {
        return new Counter { value: 42 };
    }
}

test "self_type" {
    var p = new Point { x: 1, y: 2 };
    var q = p.clone();
}

test "self_type_param" {
    var a = new Counter { value: 3 };
    var b = new Counter { value: 7 };
    var c = a.combine(b);
}

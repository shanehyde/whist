// Expected: PASS: self_type_param

trait Combinable {
    func combine(other: Self): Self;
}

struct Counter { value: i64 }

impl Combinable for Counter {
    func combine(other: Counter): Counter {
        return new Counter { value: 42 };
    }
}

test "self_type_param" {
    var a = new Counter { value: 3 };
    var b = new Counter { value: 7 };
    var c = a.combine(b);
}

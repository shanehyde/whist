// Test: const struct fields can be assigned in init() for generic types

struct Container<T> {
    items: Vec<T>,
    const capacity: i64,
}

impl Container<T> {
    func init(cap: i64) {
        self.items = new Vec<T>{};
        self.capacity = cap;
    }
}

func main() -> i32 {
    var c = new Container<i64>(10);
    return 0;
}

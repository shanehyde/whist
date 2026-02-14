trait HasValue {
    func value(): i64;
}
// Expected exit: 42

struct Wrapper {
    v: i64,
}

impl HasValue for Wrapper {
    func value(): i64 {
        return self.v;
    }
}

struct Box<T: HasValue> {
    item: T,
}

func main(): i32 {
    var w = new Wrapper {v: 42};
    var b = new Box<Wrapper> {item: w};
    return w.value();
}

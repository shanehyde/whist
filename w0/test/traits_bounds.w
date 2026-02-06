trait HasValue {
    func value(): i64;
}

struct Wrapper {
    v: i64,
}

impl HasValue for Wrapper {
    func (Wrapper) value(): i64 {
        return self.v;
    }
}

struct Box<T: HasValue> {
    item: T,
}

func main(): i32 {
    var w: Wrapper = {v: 42};
    var b: Box<Wrapper> = {item: w};
    return w.value();
}

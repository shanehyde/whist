// Valid test: method-level generic type parameters parse and type-check

struct Box<T> {
    value: T,
}

func (Box<T>) transform<K>(f: func(T) :K): K {
    return f(self.value);
}

func to_bool(x: i64): bool {
    return x > 10;
}

func main(): i32 {
    var b = new Box<i64>{value: 42};
    var result: bool = b.transform(to_bool);
    return 0;
}

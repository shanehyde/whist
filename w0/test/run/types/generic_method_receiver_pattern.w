// Expected: PASS: generic_method_specialized_receiver
// Expected: PASS: generic_method_renamed_receiver_type_param

struct Box<T> {
    value: T,
}

struct Pair<K, V> {
    key: K,
    value: V,
}

func (Pair<i32, Box<T>>) map<U>(f: func(T): U): U {
    return f(self.value.value);
}

func (Box<U>) apply<K>(f: func(U): K): K {
    return f(self.value);
}

func is_positive(x: i64): bool {
    return x > 0;
}

func add_one(x: i64): i64 {
    return x + 1;
}

test "generic_method_specialized_receiver" {
    var p =
        new Pair<i32, Box<i64>> {key: 7, value: new Box<i64> {value: 42}};
    var ok: bool = p.map(is_positive);
    assert(ok);
}

test "generic_method_renamed_receiver_type_param" {
    var b = new Box<i64> {value: 9};
    var v: i64 = b.apply(add_one);
    assert(v == 10);
}

func main(): i32 {
    return 0;
}

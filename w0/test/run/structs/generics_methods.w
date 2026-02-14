// Expected: PASS: generics_methods
// Generic struct with methods

struct Box<T> {
    value: T,
}

struct Pair<K, V> {
    key: K,
    value: V,
}

func (Box<T>) get(): T {
    return self.value;
}

func (Box<T>) set(v: T): void {
    self.value = v;
}

func (Pair<i32, Box<T>>) set(k: i32, v: Box<T>): void {
    self.key = k;
    self.value = v;
}

test "generics_methods" {
    var b = new Box<i64> {value: 10};
    var p = new Pair<i32, Box<i64>> {key: 1, value: b};

    // Test get method
    assert(b.get() == 10);

    // Test set method
    b.set(42);
    assert(b.get() == 42);
}

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

func main(): i32 {
    var b: Box<i64> = {value: 10};
    var p: Pair<i32, Box<i64>> = {key: 1, value: b};

    // Test get method
    if (b.get() != 10) {
        return 1;
    }

    // Test set method
    b.set(42);
    if (b.get() != 42) {
        return 2;
    }

    return 0;
}

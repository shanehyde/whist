// Generic struct with methods

struct Box<T> {
    value: T,
}

func (Box<T>) get(): T {
    return self.value;
}

func (Box<T>) set(v: T): void {
    self.value = v;
}

func main(): i32 {
    var b: Box<i64> = {value: 10};

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

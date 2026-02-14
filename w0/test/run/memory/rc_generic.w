// Test new with generic structs

struct Box<T> {
    value: T,
}

func (Box<T>) get(): T {
    return self.value;
}

func main(): i32 {
    var b = new Box<i64> { value: 42 };
    if (b.get() != 42) {
        return 1;
    }
    return 0;
}

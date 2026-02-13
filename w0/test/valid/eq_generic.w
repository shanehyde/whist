struct Box<T> {
    value: T,
}

trait Eq {
    func eq(other: Self): bool;
}

impl Eq for Box<T> {
    func eq(other: Box<T>): bool {
        return self.value == other.value;
    }
}

func main(): i32 {
    var a = new Box<i64> { value: 42 };
    var b = new Box<i64> { value: 42 };

    if (a == b) {}

    return 0;
}

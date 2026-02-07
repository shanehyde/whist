// Basic generic struct test

struct Box<T> {
    value: T,
}

func main(): i32 {
    var b = new Box<i64> {value: 42};
    if (b.value != 42) {
        return 1;
    }
    return 0;
}

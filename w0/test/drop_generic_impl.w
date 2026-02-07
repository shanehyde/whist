// Test: impl Drop for a generic struct type-checks
trait Drop {
    func drop(): void;
}

struct Box<T> {
    item: T,
}

impl Drop for Box {
    func (Box<T>) drop(): void {
        // cleanup
    }
}

func main(): i32 {
    var b = new Box<i64> { item: 42 };
    return 0;
}

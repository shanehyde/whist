// RC RUNTIME TEST: Drop on generic struct is called when refcount reaches 0

trait Drop {
    func drop(): void;
}

struct Box<T> {
    item: T,
}

impl Drop for Box<T> {
    func drop(): void {
        // Generic drop called
    }
}

func main(): i32 {
    var b = new Box<i64> { item: 42 };
    return 0;
}

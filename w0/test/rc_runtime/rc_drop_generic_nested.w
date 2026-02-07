// RC RUNTIME TEST: Non-generic Container holding Box<Inner> where Inner has Drop
// Exercises Bug 2 fix: generic-instance fields in non-generic __rc_dec

trait Drop {
    func drop(): void;
}

struct Inner {
    value: i64,
}

impl Drop for Inner {
    func (Inner) drop(): void {
        // Inner cleanup
    }
}

struct Box<T> {
    item: T,
}

struct Container {
    wrapped: Box<Inner>,
}

func main(): i32 {
    var inner = new Inner { value: 10 };
    var b = new Box<Inner> { item: inner };
    var c = new Container { wrapped: b };
    return 0;
}

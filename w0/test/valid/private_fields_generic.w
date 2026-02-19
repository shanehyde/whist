// Valid test: private fields on generic structs accessible from methods

struct Container<T> {
    private data: T,
}

impl Container<T> {
    func init(val: T) {
        self.data = val;
    }
}

func (Container<T>) get() -> T {
    return self.data;
}

func main() -> i32 {
    var c = new Container<i64>(42);
    var val = c.get();
    return 0;
}

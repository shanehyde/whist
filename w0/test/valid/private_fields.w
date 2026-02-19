// Valid test: private fields accessible within methods and impl blocks

struct Counter {
    private count: i64,
}

impl Counter {
    func init(initial: i64) {
        self.count = initial;
    }
}

func (Counter) get() -> i64 {
    return self.count;
}

func (Counter) increment() -> void {
    self.count = self.count + 1;
}

impl Counter {
    func reset() -> void {
        self.count = 0;
    }
}

func main() -> i32 {
    var c = new Counter(10);
    c.increment();
    c.reset();
    var val = c.get();
    return 0;
}

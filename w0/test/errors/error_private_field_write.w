// ERROR TEST: assigning to private field from outside
// Expected error: Field 'count' is private

struct Counter {
    private count: i64,
}

func main() -> i32 {
    var c = new Counter{count: 0};
    c.count = 5;
    return 0;
}

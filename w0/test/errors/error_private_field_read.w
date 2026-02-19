// ERROR TEST: accessing private field from outside
// Expected error: Field 'count' is private

struct Counter {
    private count: i64,
    value: i64,
}

func main() -> i32 {
    var c = new Counter{count: 0, value: 1};
    var x = c.count;
    return 0;
}

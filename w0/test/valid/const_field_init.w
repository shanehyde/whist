// Test: const struct fields can be assigned in init()

struct Config {
    const max_size: i64,
    name: string,
}

impl Config {
    func init(size: i64, n: string) {
        self.max_size = size;
        self.name = n;
    }
}

func main() -> i32 {
    var c = new Config(100, "test");
    return 0;
}

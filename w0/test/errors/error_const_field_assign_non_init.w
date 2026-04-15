// ERROR TEST: Cannot assign to const field outside init()
// Expected error: Cannot assign to const field 'max_size'

struct Config {
    const max_size: i64,
    name: string,
}

impl Config {
    func init(size: i64) {
        self.max_size = size;
        self.name = "default";
    }

    func reset() {
        self.max_size = 0;
    }
}

func main() -> i32 {
    var c = new Config(100);
    return 0;
}

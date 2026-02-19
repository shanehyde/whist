// Expected error: init must not have a return type

struct Point {
    x: i64,
    y: i64,
}

impl Point {
    func init(x: i64) -> i64 {
        self.x = x;
        return 0;
    }
}

func main() -> i32 {
    return 0;
}

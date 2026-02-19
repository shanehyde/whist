// Expected error: does not implement trait

trait Numeric {
    func val() -> i64;
}

struct Box<T: Numeric> {
    item: T,
}

struct Plain {
    x: i64,
}

func main() -> i32 {
    var p = new Plain {x: 1};
    var b = new Box<Plain> {item: p};
    return 0;
}

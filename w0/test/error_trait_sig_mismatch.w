// Expected error: Method 'count' return type mismatch

trait Countable {
    func count(): i64;
}

struct Bag {
    n: i64,
}

impl Countable for Bag {
    func (Bag) count(): string {
        return "oops";
    }
}

func main(): i32 {
    return 0;
}

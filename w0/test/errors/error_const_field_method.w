// ERROR TEST: Cannot call mutating method on const field
// Expected error: Cannot call mutating method 'push' on const 'v'

struct Box {
    const v: Vec<i32>,
}

func main() -> i32 {
    var b = new Box { v: new Vec<i32>{} };
    b.v.push(1);
    return 0;
}

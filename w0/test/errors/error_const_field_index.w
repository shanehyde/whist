// ERROR TEST: Cannot write to index of const field
// Expected error: Cannot write to index of const 'v'

struct Box {
    const v: Vec<i32>,
}

func main() -> i32 {
    var b = new Box { v: new Vec<i32>{} };
    b.v[0] = 5;
    return 0;
}

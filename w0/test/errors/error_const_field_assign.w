// ERROR TEST: Cannot assign to const field
// Expected error: Cannot assign to const field 'v'

struct Box {
    const v: i64,
}

func main(): i32 {
    var b = new Box { v: 42 };
    b.v = 99;
    return 0;
}

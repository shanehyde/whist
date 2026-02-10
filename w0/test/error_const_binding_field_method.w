// ERROR TEST: const binding prevents mutating method through field access
// Expected error: Cannot call mutating method 'push' on const 'v'

struct Box {
    v: Vec<i32>,
}

func main(): i32 {
    const b = new Box { v: new Vec<i32>{} };
    b.v.push(1);
    return 0;
}

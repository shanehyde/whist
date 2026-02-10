// ERROR TEST: const binding prevents index write through field access
// Expected error: Cannot write to index of const 'v'

struct Box {
    v: Vec<i32>,
}

func main(): i32 {
    const b = new Box { v: new Vec<i32>{} };
    b.v[0] = 5;
    return 0;
}

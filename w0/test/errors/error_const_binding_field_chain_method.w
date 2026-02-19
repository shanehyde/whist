// ERROR TEST: const binding propagation is transitive through member chains
// Expected error: Cannot call mutating method 'push' on const 'v'

struct Box {
    v: Vec<i32>,
}

struct Outer {
    box: Box,
}

func main() -> i32 {
    const o = new Outer { box: new Box { v: new Vec<i32>{} } };
    o.box.v.push(1);
    return 0;
}

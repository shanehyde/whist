// ERROR TEST: const field prevents mutating method even without const binding
// Expected error: Cannot call mutating method 'push' on const 'v'

import std;
use std::println;

struct Box {
    const v: Vec<i32>,
}

func main() -> i32 {
    var b = new Box { v: new Vec<i32>{} };

    b.v.push(1);

    return 0;
}

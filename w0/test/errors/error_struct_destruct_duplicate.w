// ERROR TEST: Duplicate field names in struct destructuring
// Expected error: Redefinition of 'x'

struct Point {
    x: i64,
    y: i64,
}

func main(): i32 {
    var {x, x} = new Point {x: 1, y: 2};
    return 0;
}

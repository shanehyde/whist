// ERROR TEST: Struct destructuring on non-struct type
// Expected error: Struct destructuring requires a struct type, got 'i64'

func main(): i32 {
    var {x, y} = 42;
    return 0;
}

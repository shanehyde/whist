// ERROR TEST: new with non-struct type should fail
// Expected error: 'new' requires a struct type

func main(): i32 {
    var x = new i64 { };
    return 0;
}

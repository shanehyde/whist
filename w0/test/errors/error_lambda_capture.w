// ERROR TEST: Lambda capture not yet supported
// Expected error: Cannot capture variable 'x'

func main(): i32 {
    var x = 42;
    var f: func(): i64 = || x;
    return 0;
}

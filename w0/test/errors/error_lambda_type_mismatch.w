// ERROR TEST: Lambda type mismatch
// Expected error: f: expected 'func(i64): string', got 'func(i64): i64'

func main(): i32 {
    var f: func(i64): string = |x: i64| x * 2;
    return 0;
}

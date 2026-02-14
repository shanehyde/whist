// ERROR TEST: extern function without explicit return type should fail
// Expected error: Extern function 'foo' must have an explicit return type

private extern mylib {
    func foo(x: i32);
}

func main(): i32 {
    return 0;
}

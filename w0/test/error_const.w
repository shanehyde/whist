// ERROR TEST: Assignment to const
// Expected error: Cannot assign to const 'x'

func main(): i32 {
    const x = 10;
    x = 20;
    return x;
}

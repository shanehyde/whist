// ERROR TEST: const-qualified return types still enforce underlying type checks
// Expected error: Return: expected 'i64', got 'string'

func bad(): const i64 {
    return "oops";
}

func main(): i32 {
    return 0;
}

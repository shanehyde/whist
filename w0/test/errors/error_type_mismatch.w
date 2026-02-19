// ERROR TEST: Type mismatch in return
// Expected error: Return: expected 'i32', got 'string'

func foo() -> i32 {
    return "hello";
}

func main() -> i32 {
    return foo();
}

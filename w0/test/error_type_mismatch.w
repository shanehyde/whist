// ERROR TEST: Type mismatch in return
// Expected error: Return type mismatch

func foo(): int {
    return "hello";
}

func main(): int {
    return foo();
}

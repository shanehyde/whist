// ERROR TEST: Type mismatch in return
// Expected error: Return type mismatch

func foo(): int32 {
    return "hello";
}

func main(): int32 {
    return foo();
}

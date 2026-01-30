// ERROR TEST: Type mismatch in return
// Expected error: Return type mismatch

func foo(): i32 {
    return "hello";
}

func main(): i32 {
    return foo();
}

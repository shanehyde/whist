// ERROR TEST: Invalid assignment target
// Expected error: Invalid assignment target

func foo(): i32 {
    return 1;
}

func main(): i32 {
    foo() = 2;
    return 0;
}

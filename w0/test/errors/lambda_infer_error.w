// Expected error: Cannot infer type for lambda parameter 'x'

func main() -> i32 {
    // No context to infer from — should error
    var f = |x| x + 1;
    return 0;
}

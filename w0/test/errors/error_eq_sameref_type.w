// Expected error: sameref() requires struct arguments
func main() -> i32 {
    var a = 42;
    var b = 42;
    if (sameref(a, b)) {}
    return 0;
}

// Expected error: Strings only support == and != comparison
func main(): i32 {
    if ("a" < "b") { return 1; }
    return 0;
}

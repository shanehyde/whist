// Expected error: String has no member 'foo'
func main() -> i32 {
    var s = "hello";
    var x = s.foo();
    return 0;
}

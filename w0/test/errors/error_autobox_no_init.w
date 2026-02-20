// ERROR TEST: autoboxing requires an initializer
// Expected error: Boxed variable declaration requires an initializer

func main() -> i32 {
    var ^x: i64;
    return 0;
}

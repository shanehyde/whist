// ERROR TEST: Invalid hex escape in string literal
// Expected error: Invalid hex escape in string literal

func main(): i32 {
    var s: string = "\xGG";
    return 0;
}

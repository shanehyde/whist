// ERROR TEST: Invalid hex escape in character literal
// Expected error: Invalid hex escape in character literal

func main() -> i32 {
    var x: char = '\xGG';
    return 0;
}

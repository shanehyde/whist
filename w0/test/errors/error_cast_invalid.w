// ERROR TEST: Invalid cast from string to i32
// Expected error: Cannot cast 'string' to 'i32'

func main() -> i32 {
    var s: string = "hello";
    var x: i32 = s as i32;
    return x;
}

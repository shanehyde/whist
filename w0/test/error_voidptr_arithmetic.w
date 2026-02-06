// ERROR TEST: voidptr does not support arithmetic
// Expected error: Invalid operands to 'PLUS'

func main(): i32 {
    var p: voidptr = null;
    var x = p + 1;
    return 0;
}

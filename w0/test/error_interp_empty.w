// ERROR TEST: Empty expression in interpolated string
// Expected error: Empty expression in string interpolation

func main(): i32 {
    var s = $"hello {}";
    return 0;
}

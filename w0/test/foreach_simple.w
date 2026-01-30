func main(): i32 {
    var result = 0;
    foreach (var num in 10..13) {
        result = result + num;
    }
    return result;  // Should return 10 + 11 + 12 = 33
}
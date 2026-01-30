func main(): i32 {
    var count = 0;
    foreach (var i in 5..5) {  // Empty range
        count = count + 1;
    }
    return count;  // Should return 0 (no iterations)
}
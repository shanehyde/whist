func main(): i32 {
    var count = 0;
    foreach (const i in 5..5) {  // Inclusive range
        count = count + 1;
    }
    return count;  // Should return 1
}
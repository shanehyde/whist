func main(): i32 {
    foreach (var x in 0..5) {
        if (x == 3) {
            break;
        }
        // Should stop at x=3, so only sum 0+1+2 = 3
        x = x + 1;  // This should be an error since x is the loop variable
    }
    return x; // This should also be an error since x is not in scope
}
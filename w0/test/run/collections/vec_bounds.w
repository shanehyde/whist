// Test Vec bounds checking (runtime panic, not a --check error)
// This test passes type-checking but panics at runtime with out-of-bounds access
// Expected exit: 1
// Expected stderr: Panic: Vec index 5 out of bounds

func main(): i32 {
    var nums = new Vec<i64>{10, 20, 30};
    var x = nums[5];
    return 0;
}

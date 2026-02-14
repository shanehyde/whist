// Test Vec remove bounds checking (runtime panic, not a --check error)
// Expected exit: 1
// Expected stderr: Panic: Vec remove index 3 out of bounds

func main(): i32 {
    var nums = new Vec<i64>{10, 20, 30};
    nums.remove(3);
    return 0;
}

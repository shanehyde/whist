// Test Vec insert bounds checking (runtime panic, not a --check error)
// Expected exit: 1
// Expected stderr: Panic: Vec insert index 5 out of bounds

func main(): i32 {
    var nums = new Vec<i64>{10, 20, 30};
    nums.insert(5, 99);
    return 0;
}

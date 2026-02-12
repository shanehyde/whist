// Test Vec insert bounds checking (runtime panic, not a --check error)

func main(): i32 {
    var nums = new Vec<i64>{10, 20, 30};
    nums.insert(5, 99);
    return 0;
}

// Test that readonly operations on const Vec are allowed

func main(): i32 {
    const v = new Vec<i64>{10, 20, 30};
    var c = v.count;
    var x = v[0];
    return 0;
}

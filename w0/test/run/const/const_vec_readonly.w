// Expected: PASS: const_vec_readonly
// Test that readonly operations on const Vec are allowed

test "const_vec_readonly" {
    const v = new Vec<i64>{10, 20, 30};
    var c = v.count;
    var x = v[0];
}

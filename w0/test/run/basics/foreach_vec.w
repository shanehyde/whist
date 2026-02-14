// Expected: PASS: foreach_vec
// Test foreach over Vec<i64>

test "foreach_vec" {
    var nums = new Vec<i64>{};

    nums.push(10);
    nums.push(20);
    nums.push(30);

    var total: i64 = 0;
    foreach (const n in nums) {
        total = total + n;
    }

    assert(total == 60);
}

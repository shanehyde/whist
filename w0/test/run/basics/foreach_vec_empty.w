// Expected: PASS: foreach_vec_empty
// Test foreach over empty Vec iterates zero times

test "foreach_vec_empty" {
    var nums = new Vec<i64>{};

    var count: i64 = 0;
    foreach (const n in nums) {
        count = count + 1;
    }

    assert(count == 0);
}

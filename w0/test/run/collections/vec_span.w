// Expected: PASS: vec_span
// Test Vec slicing to Span

func sum(s: Span<i64>): i64 {
    var total: i64 = 0;
    for (var i: i64 = 0; i < s.count; i += 1) {
        total = total + s[i];
    }
    return total;
}

test "vec_span" {
    var nums = new Vec<i64>{10, 20, 30, 40, 50};

    // Full slice
    var all: Span<i64> = nums[:];
    assert(sum(all) == 150);

    // Partial slice
    var mid: Span<i64> = nums[1:4];
    assert(sum(mid) == 90);

    // Start-only slice
    var tail: Span<i64> = nums[3:];
    assert(sum(tail) == 90);
}

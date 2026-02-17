// Expected: PASS: rc_chain_map_filter

import collections;

test "rc_chain_map_filter" {
    var nums = new Vec<i64>{};
    nums.push(1);
    nums.push(2);
    nums.push(3);
    nums.push(4);
    nums.push(5);

    // Chained method calls: map() intermediate must be decremented
    var result = nums.map(|x: i64| x * 3).filter(|x: i64| x > 4);
    assert(result.count == 4);
    assert(result[0] == 6);
    assert(result[1] == 9);
    assert(result[2] == 12);
    assert(result[3] == 15);
}

func main(): i32 {
    return 0;
}

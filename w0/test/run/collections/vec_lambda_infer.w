// Expected: PASS: map_infer
// Expected: PASS: filter_infer
// Expected: PASS: chain_infer

import collections;

test "map_infer" {
    var nums = new Vec<i64>{};
    nums.push(1);
    nums.push(2);
    nums.push(3);
    var doubled = nums.map(|x| x * 2);
    assert(doubled.count == 3);
    assert(doubled[0] == 2);
    assert(doubled[1] == 4);
    assert(doubled[2] == 6);
}

test "filter_infer" {
    var nums = new Vec<i64>{};
    nums.push(1);
    nums.push(2);
    nums.push(3);
    nums.push(4);
    var evens = nums.filter(|x| x % 2 == 0);
    assert(evens.count == 2);
    assert(evens[0] == 2);
    assert(evens[1] == 4);
}

test "chain_infer" {
    var nums = new Vec<i64>{};
    nums.push(1);
    nums.push(2);
    nums.push(3);
    nums.push(4);
    nums.push(5);
    var result = nums.map(|x| x * 3).filter(|x| x > 4);
    assert(result.count == 4);
    assert(result[0] == 6);
    assert(result[1] == 9);
    assert(result[2] == 12);
    assert(result[3] == 15);
}

func main(): i32 {
    return 0;
}

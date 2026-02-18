// Expected: PASS: vec_map_basic
// Expected: PASS: vec_map_type_transform
// Expected: PASS: vec_filter_basic
// Expected: PASS: vec_each_basic

import collections;

func times_two(x: i64): i64 {
    return x * 2;
}

func is_big(x: i64): bool {
    return x > 15;
}

test "vec_map_basic" {
    var nums = new Vec<i64>{};
    nums.push(1);
    nums.push(2);
    nums.push(3);

    var doubled = nums.map(|x: i64| x * 2);
    assert(doubled.count == 3);
    assert(doubled[0] == 2);
    assert(doubled[1] == 4);
    assert(doubled[2] == 6);
}

test "vec_map_type_transform" {
    var nums = new Vec<i64>{};
    nums.push(10);
    nums.push(20);

    // map<K> with i64 -> bool (different type)
    var flags = nums.map(is_big);
    assert(flags.count == 2);
    assert(flags[0] == false);
    assert(flags[1] == true);
}

test "vec_filter_basic" {
    var nums = new Vec<i64>{};
    nums.push(1);
    nums.push(2);
    nums.push(3);
    nums.push(4);
    nums.push(5);

    var evens = nums.filter(|x: i64| x % 2 == 0);
    assert(evens.count == 2);
    assert(evens[0] == 2);
    assert(evens[1] == 4);
}

test "vec_each_basic" {
    var nums = new Vec<i64>{};
    nums.push(1);
    nums.push(2);
    nums.push(3);
    nums.push(4);
    nums.push(5);

    var collected = new Vec<i64>{};
    nums.each(|x| collected.push(x));
    assert(collected.count == 5);
    assert(collected[0] == 1);
    assert(collected[4] == 5);
}

func main(): i32 {
    return 0;
}

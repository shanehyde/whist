// Expected: PASS: vec_map_with_capture
// Expected: PASS: vec_filter_with_capture

import collections;

test "vec_map_with_capture" {
    var v = new Vec<i64>{};
    v.push(1);
    v.push(2);
    v.push(3);
    var factor: i64 = 10;
    var result = v.map(|x: i64| x * factor);
    assert(result.count == 3);
    assert(result[0] == 10);
    assert(result[1] == 20);
    assert(result[2] == 30);
}

test "vec_filter_with_capture" {
    var v = new Vec<i64>{};
    v.push(1);
    v.push(2);
    v.push(3);
    v.push(4);
    v.push(5);
    var threshold: i64 = 3;
    var result = v.filter(|x: i64| x > threshold);
    assert(result.count == 2);
    assert(result[0] == 4);
    assert(result[1] == 5);
}

func main(): i32 {
    return 0;
}

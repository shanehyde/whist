// Expected: PASS: vec_nested
// Test nested Vec<Vec<i64>> -- create, push, index, count

test "vec_nested" {
    var outer = new Vec<Vec<i64>>{};

    var inner1 = new Vec<i64>{1, 2, 3};
    var inner2 = new Vec<i64>{4, 5};

    outer.push(inner1);
    outer.push(inner2);

    assert(outer.count == 2);
    assert(outer[0].count == 3);
    assert(outer[1].count == 2);
    assert(outer[0][0] == 1);
    assert(outer[0][2] == 3);
    assert(outer[1][1] == 5);

    // Push to inner vec after it's in outer
    outer[0].push(10);

    assert(outer[0].count == 4);
    assert(outer[0][3] == 10);

    // outer goes out of scope -- should __rc_dec each inner Vec
}

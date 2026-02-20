// Expected: PASS: box_basic
// Expected: PASS: box_arithmetic
// Expected: PASS: box_comparison
// Expected: PASS: box_assign_through
// Expected: PASS: box_compound_assign
// Expected: PASS: box_sharing
// Expected: PASS: box_value_copy
// Expected: PASS: box_func_call
// Expected: PASS: box_constructor
// Expected: PASS: box_multiple_types

func takes_i64(x: i64) -> i64 {
    return x * 2;
}

func takes_bool(x: bool) -> bool {
    return !x;
}

test "box_basic" {
    var b = new Box<i64>{value: 42};
    assert(b == 42);
}

test "box_arithmetic" {
    var b = new Box<i64>{value: 10};
    assert(b + 5 == 15);
    assert(b * 2 == 20);
    assert(b - 3 == 7);
    assert(b / 2 == 5);
    assert(b % 3 == 1);
}

test "box_comparison" {
    var b = new Box<i64>{value: 42};
    assert(b == 42);
    assert(b != 0);
    assert(b > 0);
    assert(b >= 42);
    assert(b < 100);
    assert(b <= 42);
}

test "box_assign_through" {
    var b = new Box<i64>{value: 10};
    b = 20;
    assert(b == 20);
}

test "box_compound_assign" {
    var b = new Box<i64>{value: 10};
    b += 5;
    assert(b == 15);
    b -= 3;
    assert(b == 12);
    b *= 2;
    assert(b == 24);
}

test "box_sharing" {
    var b = new Box<i64>{value: 10};
    var c = b;
    b = 42;
    // c shares the same box, so it should see the mutation
    assert(c == 42);
}

test "box_value_copy" {
    var b = new Box<i64>{value: 42};
    var y: i64 = b;
    b = 99;
    // y is a copy, should not see the mutation
    assert(y == 42);
    assert(b == 99);
}

test "box_func_call" {
    var b = new Box<i64>{value: 21};
    var result = takes_i64(b);
    assert(result == 42);
}

test "box_constructor" {
    var b = new Box<i64>(100);
    assert(b == 100);
    b = 200;
    assert(b == 200);
}

test "box_multiple_types" {
    var bi = new Box<i32>{value: 10};
    assert(bi + 5 == 15);

    var bf = new Box<f64>{value: 3.14};
    assert(bf > 3.0);
    assert(bf < 4.0);

    var bb = new Box<bool>{value: true};
    assert(takes_bool(bb) == false);
}

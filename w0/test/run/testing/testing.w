// Expected: PASS: addition
// Expected: PASS: subtraction
// Expected: PASS: boolean checks
// Expected: FAIL: assert failure
// Expected: ASSERT FAILED: ((1 + 1) == 3)
// Expected: 3 passed, 1 failed, 4 total

// --- test_basic ---

func add(a: i64, b: i64): i64 {
    return a + b;
}

func sub(a: i64, b: i64): i64 {
    return a - b;
}

test "addition" {
    assert(add(2, 3) == 5);
    assert(add(0, 0) == 0);
    assert(add(-1, 1) == 0);
}

test "subtraction" {
    assert(sub(5, 3) == 2);
    assert(sub(0, 0) == 0);
}

test "boolean checks" {
    assert(true);
    assert(!false);
    assert(1 + 1 == 2);
}

// --- test_assert_fail ---

// test "assert failure" {
//     assert(1 + 1 == 3);
// }

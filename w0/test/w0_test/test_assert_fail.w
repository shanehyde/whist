// Expected: FAIL: assert failure
// Expected: ASSERT FAILED: ((1 + 1) == 3)
// Expected: 0 passed, 1 failed, 1 total

test "assert failure" {
    assert(1 + 1 == 3);
}

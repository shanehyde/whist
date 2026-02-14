// Expected: PASS: foreach_simple

test "foreach_simple" {
    var result = 0;
    var z = 2;

    foreach (const num in 10..14 by z) {
        result = result + num;
    }
    assert(result == 22);  // 10 + 12 = 22 (14 excluded)
}

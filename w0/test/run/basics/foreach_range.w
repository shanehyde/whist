// Expected: PASS: foreach_range

test "foreach_range" {
    var total = 0;
    foreach (const i in 1..6) {
        total = total + i;
    }
    assert(total == 15);
}

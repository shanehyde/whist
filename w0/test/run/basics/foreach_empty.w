// Expected: PASS: foreach_empty

test "foreach_empty" {
    var count = 0;
    foreach (const i in 5..5) {  // Exclusive: 5..5 is empty range
        count = count + 1;
    }
    assert(count == 0);
}

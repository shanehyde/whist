// Expected: PASS: foreach_span
// Test foreach over Span<i64>

test "foreach_span" {
    var arr: [5]i64;
    arr[0] = 10;
    arr[1] = 20;
    arr[2] = 30;
    arr[3] = 40;
    arr[4] = 50;

    var s: Span<i64> = arr[:];

    var total: i64 = 0;
    foreach (const n in s) {
        total = total + n;
    }

    assert(total == 150);

    // Foreach over a slice of the span
    var s2: Span<i64> = arr[1:4];
    var total2: i64 = 0;
    foreach (const n in s2) {
        total2 = total2 + n;
    }

    assert(total2 == 90);

    // Foreach over empty span
    var empty: Span<i64> = arr[0:0];
    var total3: i64 = 0;
    foreach (const n in empty) {
        total3 = total3 + n;
    }

    assert(total3 == 0);
}

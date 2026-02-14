// Test foreach over Span<i64>

func main(): i32 {
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

    if (total != 150) {
        return 1;
    }

    // Foreach over a slice of the span
    var s2: Span<i64> = arr[1:4];
    var total2: i64 = 0;
    foreach (const n in s2) {
        total2 = total2 + n;
    }

    if (total2 != 90) {
        return 2;
    }

    // Foreach over empty span
    var empty: Span<i64> = arr[0:0];
    var total3: i64 = 0;
    foreach (const n in empty) {
        total3 = total3 + n;
    }

    if (total3 != 0) {
        return 3;
    }

    return 0;
}

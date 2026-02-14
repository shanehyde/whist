// Test slice syntax variants
func main(): i32 {
    var arr: [10]i64;
    arr[0] = 10;
    arr[1] = 20;
    arr[2] = 30;
    arr[3] = 40;
    arr[4] = 50;
    arr[5] = 60;
    arr[6] = 70;
    arr[7] = 80;
    arr[8] = 90;
    arr[9] = 100;

    // Test [:] - full slice
    var full: Span<i64> = arr[:];
    if (full.count != 10) { return 1; }
    if (full[0] != 10) { return 2; }
    if (full[9] != 100) { return 3; }

    // Test [start:end]
    var middle: Span<i64> = arr[3:7];
    if (middle.count != 4) { return 4; }
    if (middle[0] != 40) { return 5; }
    if (middle[3] != 70) { return 6; }

    // Test [start:]
    var tail: Span<i64> = arr[7:];
    if (tail.count != 3) { return 7; }
    if (tail[0] != 80) { return 8; }
    if (tail[2] != 100) { return 9; }

    // Test [:end]
    var head: Span<i64> = arr[:4];
    if (head.count != 4) { return 10; }
    if (head[0] != 10) { return 11; }
    if (head[3] != 40) { return 12; }

    // Slice of a span
    var sub: Span<i64> = full[2:8];
    if (sub.count != 6) { return 13; }
    if (sub[0] != 30) { return 14; }

    var subsub: Span<i64> = sub[1:4];
    if (subsub.count != 3) { return 15; }
    if (subsub[0] != 40) { return 16; }
    if (subsub[2] != 60) { return 17; }

    return 0;
}

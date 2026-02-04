func main(): i32 {
    var arr: [5]i64;
    arr[0] = 1;
    arr[1] = 2;
    arr[2] = 3;
    arr[3] = 4;
    arr[4] = 5;

    var s: Span<i64> = arr[:];
    if (s.count != 5) { return 1; }
    if (s[0] != 1) { return 2; }
    if (s[4] != 5) { return 3; }

    var s2: Span<i64> = arr[1:4];
    if (s2.count != 3) { return 4; }
    if (s2[0] != 2) { return 5; }

    var s3: Span<i64> = s2[1:];
    if (s3.count != 2) { return 6; }

    var empty: Span<i64> = arr[0:0];
    if (empty.count != 0) { return 7; }

    return 0;
}

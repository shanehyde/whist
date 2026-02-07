// RC RUNTIME TEST: Span<T> fields are value types (no RC cleanup).

struct Buffer {
    data: Span<i64>,
    count: i64,
}

func main(): i32 {
    var arr: [3]i64;
    arr[0] = 1;
    arr[1] = 2;
    arr[2] = 3;

    var s: Span<i64> = arr[:];
    var buf = new Buffer { data: s, count: 3 };

    if (buf.count != 3) {
        return 1;
    }
    if (buf.data.count != 3) {
        return 2;
    }
    if (buf.data[1] != 2) {
        return 3;
    }

    return 0;
}

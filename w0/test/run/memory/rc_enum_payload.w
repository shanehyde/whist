// Expected: PASS: rc_enum_payload
// Test RC handling for enum payloads

struct Point {
    x: i64,
    y: i64,
}

enum MaybePoint {
    None,
    Some(Point),
}

struct Holder {
    value: MaybePoint,
}

test "rc_enum_payload" {
    var p = new Point { x: 1, y: 2 };
    var m = MaybePoint::Some(p);
    var h = new Holder { value: m };

    h.value = MaybePoint::None;
    m = MaybePoint::None;
    m = MaybePoint::Some(p);
}

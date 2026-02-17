// Expected: PASS: struct_destructure_rename
// Test struct destructuring with field renaming: var {field: local} = expr;

struct Point {
    x: i64,
    y: i64,
}

struct Info {
    code: i64,
    value: i64,
    extra: i64,
}

func make_point(a: i64, b: i64): Point {
    return new Point {x: a, y: b};
}

test "struct_destructure_rename" {
    // Basic: rename both fields
    var {x: px, y: py} = new Point {x: 10, y: 20};
    assert(px == 10);
    assert(py == 20);

    // Mix renamed and non-renamed fields
    var {code, value: val} = new Info {code: 42, value: 7, extra: 99};
    assert(code == 42);
    assert(val == 7);

    // Const with rename
    const {x: cx, y: cy} = new Point {x: 30, y: 40};
    assert(cx == 30);
    assert(cy == 40);

    // Partial destructure with rename
    var {extra: e} = new Info {code: 1, value: 2, extra: 3};
    assert(e == 3);

    // Destructure from function return with rename
    if (true) {
        var {x: fx, y: fy} = make_point(50, 60);
        assert(fx == 50);
        assert(fy == 60);
    }
}

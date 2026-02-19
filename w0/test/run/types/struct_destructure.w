// Expected: PASS: struct_destructure
// Test struct destructuring: var {field1, field2} = expr;

struct Point {
    x: i64,
    y: i64,
}

func make_point(a: i64, b: i64) -> Point {
    return new Point {x: a, y: b};
}

struct Info {
    code: i64,
    value: i64,
    extra: i64,
}

test "struct_destructure" {
    // Basic: destructure from new expression
    var {x, y} = new Point {x: 10, y: 20};
    assert(x == 10);
    assert(y == 20);

    // Partial: only extract some fields
    var {code} = new Info {code: 42, value: 7, extra: 99};
    assert(code == 42);

    // Const destructuring
    const {value, extra} = new Info {code: 1, value: 2, extra: 3};
    assert(value == 2);
    assert(extra == 3);

    // Destructure from function return (RC-tracked temp)
    if (true) {
        var {x, y} = make_point(30, 40);
        assert(x == 30);
        assert(y == 40);
    }
}

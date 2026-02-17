// Expected: PASS: closure_capture_string
// Expected: PASS: closure_capture_struct
// Expected: PASS: closure_capture_vec
// Expected: PASS: closure_capture_closure_value

import std;

struct Point { x: i64, y: i64 }

func apply_s(f: func(string): string, x: string): string {
    return f(x);
}

func apply_i(f: func(i64): i64, x: i64): i64 {
    return f(x);
}

func get_x(p: Point): i64 {
    return p.x;
}

test "closure_capture_string" {
    var greeting: string = "hello";
    var f = |x: string| -> string x + " " + greeting;
    var result = apply_s(f, "say");
    assert(result == "say hello");
}

test "closure_capture_struct" {
    var p = new Point { x: 10, y: 20 };
    var f = |dx: i64| -> i64 get_x(p) + dx;
    assert(apply_i(f, 5) == 15);
    assert(apply_i(f, 0) == 10);
}

test "closure_capture_vec" {
    var v = new Vec<i64>{};
    v.push(1);
    v.push(2);
    v.push(3);
    var f = |i: i64| -> i64 v.count + i;
    assert(apply_i(f, 0) == 3);
    assert(apply_i(f, 7) == 10);
}

test "closure_capture_closure_value" {
    var h: func(i64): i64 = null;
    {
        var x: i64 = 10;
        var f = |y: i64| -> i64 x + y;
        h = |z: i64| -> i64 f(z) + 1;
    }
    assert(apply_i(h, 5) == 16);
}

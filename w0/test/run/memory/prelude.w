// Expected: PASS: eq_runtime
// Expected: PASS: prelude_result_option_queries
// Expected: PASS: prelude_value_extract
// Test value equality via Eq trait and sameref
// Test Option/Result query methods provided by prelude
// Test Option/Result extraction methods at runtime

struct Point {
    x: i64,
    y: i64,
}

trait Eq {
    func eq(other: Self) -> bool;
}

impl Eq for Point {
    func eq(other: Point) -> bool {
        return self.x == other.x && self.y == other.y;
    }
}

enum Shape {
    Circle(i64),
    Rect(i64, i64),
    None,
}

test "eq_runtime" {
    // Struct equality
    var a = new Point { x: 1, y: 2 };
    var b = new Point { x: 1, y: 2 };
    var c = new Point { x: 3, y: 4 };

    assert(a == b);
    assert(!(a != b));
    assert(!(a == c));
    assert(a != c);

    // sameref: a and b are different allocations
    assert(!sameref(a, b));
    var d = a;
    assert(sameref(a, d));

    // Data enum equality
    var s1 = Shape::Circle(5);
    var s2 = Shape::Circle(5);
    var s3 = Shape::Rect(1, 2);
    var s4 = Shape::None;
    var s5 = Shape::None;

    assert(s1 == s2);
    assert(!(s1 == s3));
    assert(!(s1 == s4));
    assert(s4 == s5);
}

test "prelude_result_option_queries" {
    var opt_some: Option<i64> = Option::Some(42);
    var opt_none: Option<i64> = Option::None;

    assert(opt_some.has_value());
    assert(!opt_none.has_value());

    var res_ok: Result<i64, string> = Result::Ok(42);
    var res_err: Result<i64, string> = Result::Err("bad");

    assert(res_ok.has_value());
    assert(!res_err.has_value());

    assert(res_ok.is_ok());
    assert(!res_err.is_ok());

    assert(!res_ok.is_err());
    assert(res_err.is_err());
}

test "prelude_value_extract" {
    // Option.value() on Some
    var opt_some: Option<i64> = Option::Some(42);
    assert(opt_some.value() == 42);

    // Option.expect() on Some
    assert(opt_some.expect("unreachable") == 42);

    // Option.value_or() on Some returns inner value
    assert(opt_some.value_or(99) == 42);

    // Option.value_or() on None returns default
    var opt_none: Option<i64> = Option::None;
    assert(opt_none.value_or(99) == 99);

    // Result.value() on Ok
    var res_ok: Result<i64, string> = Result::Ok(100);
    assert(res_ok.value() == 100);

    // Result.expect() on Ok
    assert(res_ok.expect("unreachable") == 100);

    // Result.value_or() on Ok returns inner value
    assert(res_ok.value_or(0) == 100);

    // Result.value_or() on Err returns default
    var res_err: Result<i64, string> = Result::Err("bad");
    assert(res_err.value_or(0) == 0);

    // Result.error() on Err
    var res_err2: Result<i64, string> = Result::Err("error_msg");
    var e: string = res_err2.error();

    // Option with string type
    var opt_str: Option<string> = Option::Some("hello");
    assert(opt_str.value_or("default") == "hello");

    var opt_str_none: Option<string> = Option::None;
    assert(opt_str_none.value_or("default") == "default");
}

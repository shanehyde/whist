// Expected: PASS: if expr order

test "if expr order" {
    var v: Option<string> = Option::None;
    assert(v.has_value() == false);
    assert( !(v.has_value() && v.value() == "hello"));
}
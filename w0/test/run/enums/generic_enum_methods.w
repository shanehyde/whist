// Expected: PASS: generic_enum_methods
// Test generic enum method instantiation/dispatch

enum Flag<T> {
    Off,
    On(T),
}

func (const Flag<T>) has_value() -> bool {
    match (self) {
        On(_) => return true;
        Off => return false;
    }
}

test "generic_enum_methods" {
    var on: Flag<i64> = Flag::On(42);
    var off: Flag<i64> = Flag::Off;

    assert(on.has_value());
    assert(!off.has_value());
}

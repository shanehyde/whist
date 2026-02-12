// Test generic enum method instantiation/dispatch

enum Flag<T> {
    Off,
    On(T),
}

func (const Flag<T>) has_value(): bool {
    match (self) {
        On(_) => return true;
        Off => return false;
    }
}

func main(): i32 {
    var on: Flag<i64> = Flag::On(42);
    var off: Flag<i64> = Flag::Off;

    if (!on.has_value()) {
        return 1;
    }
    if (off.has_value()) {
        return 2;
    }

    return 0;
}

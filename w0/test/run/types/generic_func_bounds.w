// Expected: PASS: generic_func_bounds

trait Printable {
    const func label() -> string;
}

struct Dog {
    name: string,
}

impl Printable for Dog {
    const func label() -> string {
        return self.name;
    }
}

func get_label<T: Printable>(x: T) -> string {
    return x.label();
}

test "generic_func_bounds" {
    var d = new Dog{name: "Rex"};
    var s = get_label(d);
    assert(s == "Rex");
}

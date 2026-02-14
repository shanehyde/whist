trait Printable {
    const func label(): string;
}

struct Dog {
    name: string,
}

impl Printable for Dog {
    const func label(): string {
        return self.name;
    }
}

func get_label<T: Printable>(x: T): string {
    return x.label();
}

func main(): i32 {
    var d = new Dog{name: "Rex"};
    var s = get_label(d);
    if (s != "Rex") { return 1; }
    return 0;
}

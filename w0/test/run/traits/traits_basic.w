// Expected: PASS: traits_basic

trait Greetable {
    func greet(): string;
    const func name_length(): i64;
}

struct Dog {
    name: string,
}

impl Greetable for Dog {
    func greet(): string {
        return self.name;
    }
    const func name_length(): i64 {
        return 3;
    }
}

test "traits_basic" {
    var d: Dog = new Dog{name: "Rex\n"};
    var s: string = d.greet();
}

// Expected: PASS: duck_type_basic
// Expected: PASS: duck_type_mixed
import std;

trait Greetable {
    func greet(): string;
}

struct Dog {
    name: string,
}

// Signature-only: asserts Dog has greet() via standalone method below
impl Greetable for Dog {
    func greet(): string;
}

// Standalone receiver method provides the actual body
func (Dog) greet(): string {
    return self.name;
}

func hello<T: Greetable>(x: T): string {
    return x.greet();
}

// Mixed impl block: some methods with bodies, some signature-only

trait Animal {
    func speak(): string;
    const func legs(): i64;
}

struct Cat {
    name: string,
}

impl Animal for Cat {
    // Signature-only: body provided by standalone method below
    func speak(): string;

    // Body provided inline
    const func legs(): i64 {
        return 4;
    }
}

func (Cat) speak(): string {
    return self.name;
}

test "duck_type_basic" {
    var d = new Dog { name: "hello from Dog" };
    assert("hello from Dog" == hello(d));
}

test "duck_type_mixed" {
}

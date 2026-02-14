// Expected: PASS: duck_type_basic
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

test "duck_type_basic" {
    var d = new Dog { name: "hello from Dog" };
    assert("hello from Dog" == hello(d));
}
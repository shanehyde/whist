// Expected: hello from Dog
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

func main(): i32 {
    var d = new Dog { name: "hello from Dog\n" };
    std.print(d.greet());
    return 0;
}

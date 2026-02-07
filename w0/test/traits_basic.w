import std;

trait Greetable {
    func greet(): string;
}

struct Dog {
    name: string,
}

impl Greetable for Dog {
    func (Dog) greet(): string {
        return self.name;
    }
}

func main(): i32 {
    var d: Dog = new Dog {name: "Rex\n"};
    var s: string = d.greet();
    std.print(s);
    return 0;
}

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
    var d: Dog = {name: "Rex"};
    var s: string = d.greet();
    return 0;
}

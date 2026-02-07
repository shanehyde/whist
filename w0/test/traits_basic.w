import std;

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

func main(): i32 {
    var d: Dog = new Dog {name: "Rex\n"};
    var s: string = d.greet();
    std.print(s);
    return 0;
}

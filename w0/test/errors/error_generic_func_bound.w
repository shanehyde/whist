trait Printable {
    const func label() -> string;
}

struct Cat {
    name: string,
}

func get_label<T: Printable>(x: T) -> string {
    return x.label();
}

func main() -> i32 {
    var c = new Cat{name: "Whiskers"};
    // Expected error: Type 'Cat' does not implement trait 'Printable' required by 'get_label'
    var s = get_label(c);
    return 0;
}

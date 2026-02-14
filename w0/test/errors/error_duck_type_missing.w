// Expected error: No standalone method 'greet' found for type 'Dog'

trait Greetable {
    func greet(): string;
}

struct Dog {
    name: string,
}

// Signature-only but no standalone method defined anywhere
impl Greetable for Dog {
    func greet(): string;
}

func main(): i32 {
    return 0;
}

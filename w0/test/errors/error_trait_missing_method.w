// Expected error: Missing required method 'to_string' from trait 'Printable'

trait Printable {
    func to_string(): string;
}

struct Empty {}

impl Printable for Empty {}

func main(): i32 {
    return 0;
}

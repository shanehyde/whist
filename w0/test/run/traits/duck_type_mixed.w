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

func main(): i32 {
    return 0;
}

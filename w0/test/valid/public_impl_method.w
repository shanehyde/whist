// Valid test: public/private visibility modifiers on impl methods

struct Foo {
    x: i64,
}

impl Foo {
    public func init(x: i64) {
        self.x = x;
    }

    private func secret() -> i64 {
        return self.x * 2;
    }

    func default_private() -> i64 {
        return self.x;
    }
}

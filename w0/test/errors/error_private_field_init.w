// ERROR TEST: initializing private field from outside
// Expected error: Field 'secret' is private

struct Wrapper {
    public_val: i64,
    private secret: i64,
}

func (Wrapper) get_secret() -> i64 {
    return self.secret;
}

func main() -> i32 {
    var w = new Wrapper{public_val: 1, secret: 42};
    return 0;
}

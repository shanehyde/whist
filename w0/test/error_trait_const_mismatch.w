// Expected error: receiver mutability mismatch

trait Readable {
    const func read(): i64;
}

struct Sensor {
    val: i64,
}

impl Readable for Sensor {
    func read(): i64 {
        return self.val;
    }
}

func main(): i32 {
    return 0;
}

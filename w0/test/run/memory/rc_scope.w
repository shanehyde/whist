// Test RC cleanup at nested scope boundaries

struct Counter {
    value: i64,
}

func main(): i32 {
    var result: i64 = 0;
    {
        var c = new Counter { value: 10 };
        result = c.value;
        // c is decremented here at block exit
    }
    if (result != 10) {
        return 1;
    }
    return 0;
}

struct Callback { handler: func(i64): i64 }

func twice(x: i64): i64 {
    return x * 2;
}

func main(): i32 {
    var cb = new Callback { handler: twice };
    var result = cb.handler(5);
    return 0;
}

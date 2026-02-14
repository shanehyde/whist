func identity<T>(x: T): T {
    return x;
}

func main(): i32 {
    var a = identity(42);
    var b = identity("hello");
    var c = identity(true);

    if (a != 42) { return 1; }
    if (b != "hello") { return 2; }
    if (c != true) { return 3; }

    return 0;
}

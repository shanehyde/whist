func first<A, B>(a: A, b: B): A {
    return a;
}

func second<A, B>(a: A, b: B): B {
    return b;
}

func main(): i32 {
    var a = first(10, "hello");
    var b = second(10, "world");

    if (a != 10) { return 1; }
    if (b != "world") { return 2; }

    return 0;
}

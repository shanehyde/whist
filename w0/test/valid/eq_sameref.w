struct Obj {
    val: i64,
}

func main(): i32 {
    var a = new Obj { val: 1 };
    var b = a;
    var c = new Obj { val: 1 };

    if (sameref(a, b)) {}
    if (!sameref(a, c)) {}

    return 0;
}

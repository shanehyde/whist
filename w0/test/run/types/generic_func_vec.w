func vec_count<T>(v: Vec<T>): i64 {
    return v.count;
}

func main(): i32 {
    var v = new Vec<i64>{1, 2, 3};

    var n = vec_count(v);
    if (n != 3) { return 1; }

    return 0;
}

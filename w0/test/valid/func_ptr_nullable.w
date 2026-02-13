func noop(): void {
    return;
}

func main(): i32 {
    var fp: func(): void = null;
    fp = noop;
    return 0;
}

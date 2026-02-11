enum Bad {
    Foo(i32) = 1,
}

func main(): i32 {
    return 0;
}
// Expected error: Enum variant with data cannot have explicit integer value

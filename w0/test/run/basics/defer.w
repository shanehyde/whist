func cleanup(): void {
}

func test_single_defer(): i64 {
    defer cleanup();
    return 42;
}

func test_multiple_defers(): i64 {
    defer cleanup();
    defer cleanup();
    defer cleanup();
    return 100;
}

func test_void_defer(): void {
    defer cleanup();
}

func main(): i32 {
    var result = test_single_defer();
    if (result != 42) {
        return 1;
    }

    var result2 = test_multiple_defers();
    if (result2 != 100) {
        return 3;
    }

    test_void_defer();

    return 0;
}

var cleanup_count: i64 = 0;

func cleanup(): void {
    cleanup_count = cleanup_count + 1;
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
    cleanup_count = 0;

    var result = test_single_defer();
    if (result != 42) {
        return 1;
    }
    if (cleanup_count != 1) {
        return 2;
    }

    cleanup_count = 0;
    var result2 = test_multiple_defers();
    if (result2 != 100) {
        return 3;
    }
    if (cleanup_count != 3) {
        return 4;
    }

    cleanup_count = 0;
    test_void_defer();
    if (cleanup_count != 1) {
        return 5;
    }

    return 0;
}

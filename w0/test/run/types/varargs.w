extern stdio {
    func printf(fmt: string, ...): i32;
}

func main(): i32 {
    // Call printf with various argument counts
    printf("hello %s\n", "world");
    printf("number: %d\n", 42);
    printf("%d + %d = %d\n", 1, 2, 3);

    // No args beyond format string
    printf("varargs test passed\n");

    return 0;
}

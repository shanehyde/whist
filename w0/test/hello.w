// Basic hello world style program

extern stdio {
    func printf(s: string): void;
}

func main(): i32 {
    printf("Hello, world!\n");
    return 0;
}

// Test extern function aliasing with 'as' keyword
extern stdio {
    func printf(fmt: string, ...): i32 as print_formatted;
}

func main(): i32 {
    print_formatted("hello %d\n", 42);
    return 0;
}

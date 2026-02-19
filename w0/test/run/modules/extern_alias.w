// Expected: PASS: extern_alias
// Test extern function aliasing with 'as' keyword
extern stdio {
    func printf(fmt: string, ...) -> i32 as print_formatted;
}

test "extern_alias" {
    print_formatted("hello %d\n", 42);
}

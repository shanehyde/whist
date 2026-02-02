// Test string interpolation with backtick strings

extern stdio {
    func printf(s: string): void;
    func free(p: string): void;
}

func main(): i32 {
    // Simple interpolation
    var name = "World";
    var greeting = `Hello, ${name}!`;
    printf(greeting);
    printf("\n");
    free(greeting);

    // Integer interpolation
    var count = 42;
    var msg = `Count: ${count}`;
    printf(msg);
    printf("\n");
    free(msg);

    // Expression interpolation
    var a = 10;
    var b = 20;
    var sum_msg = `Sum: ${a + b}`;
    printf(sum_msg);
    printf("\n");
    free(sum_msg);

    // Bool interpolation
    var flag = true;
    var bool_msg = `Flag is ${flag}`;
    printf(bool_msg);
    printf("\n");
    free(bool_msg);

    // Simple backtick string without interpolation
    var simple = `Just a simple string`;
    printf(simple);
    printf("\n");

    // Multiple interpolations
    var x = 5;
    var y = 7;
    var multi = `x=${x}, y=${y}, sum=${x + y}`;
    printf(multi);
    printf("\n");
    free(multi);

    return 0;
}

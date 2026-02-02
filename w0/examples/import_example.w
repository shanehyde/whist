// Example demonstrating the import statement

import std;

extern stdio {
    func printf(s: string): void;
}

func main(): i32 {
    printf("Demonstrating std library import\n");

    var x = abs_i64(-100);
    var larger = max_i64(42, 99);
    var smaller = min_i64(42, 99);

    printf("Import test complete\n");
    return 0;
}

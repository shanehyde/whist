// Test that using a symbol that conflicts with a local definition gives an error
// Expected error: Redefinition of 'print'

import std;
use std.print;

func print(s: string): void {
    return;
}

func main(): i32 {
    return 0;
}

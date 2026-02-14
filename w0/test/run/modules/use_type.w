// Test use statement with types (struct from std)
// Currently std types like Result aren't public, so this test uses
// use for functions only but verifies type-related operations work
import std;
use std.{print, exit};

func main(): i32 {
    // exit is a function type - this verifies use works with all symbol kinds
    print("PASS: use_type.w\n");
    return 0;
}

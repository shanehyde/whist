// Test that use without prior import gives an error
// Expected error: Module 'std' is not imported

use std::print;

func main(): i32 {
    return 0;
}

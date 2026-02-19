// Test that using a symbol not in the module gives an error
// Expected error: No public symbol 'nonexistent' in module 'std'

import std;
use std::nonexistent;

func main() -> i32 {
    return 0;
}

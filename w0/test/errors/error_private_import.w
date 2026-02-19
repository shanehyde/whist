// Test that private symbols from library imports are not accessible via module qualification
// Expected error: Module 'std' has no public symbol '_internal_std'

import std;

func main() -> i32 {
    // This should work - abs_i64 is public in std
    var a = std::abs_i64(-5);

    // This should FAIL - _internal_std is private in std
    var b = std::_internal_std();

    return 0;
}

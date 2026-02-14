// Test that library symbols cannot be accessed without module qualification
// Expected error: Undefined identifier 'print'

import std;

func main(): i32 {
    // This should FAIL - print is from std library and requires qualification
    print("This should not compile\n");

    return 0;
}

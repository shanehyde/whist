// Expected: PASS: rc_strings
// Test RC string memory management

import std;

// impl Drop for Vec<string> {
//     func drop() {
//         // Clear should free strings
//         self.clear();
//     }
// }

test "rc_strings2" {
    // Vec<string> with dynamic strings
    var parts = "one,two,three".split(",");
    assert(parts.count == 3);
    assert(parts[0] == "one");
    assert(parts[1] == "two");
    assert(parts[2] == "three");
    // parts.clear(); // Clear should free strings
}

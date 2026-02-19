// Test: struct destructuring of module call results tracks RC for cleanup
// Bug: std::exec(cmd) is parsed as NODE_ENUM_VALUE (is_module_call=1),
//      but the destructuring path only handled NODE_CALL.

import std;

// Expected: ok

func main() -> i32 {
    // Struct destructuring of a module call result — must track the temp struct
    var {output, exit_code} = std::exec("echo hello");
    if (exit_code == 0) {
        std::println("ok");
    }
    return 0;
}

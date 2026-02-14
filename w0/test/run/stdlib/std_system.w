// Expected: PASS: std_system
import std;

test "std_system" {
    assert(std.system("echo hello") == 0);
}

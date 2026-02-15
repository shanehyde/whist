import std;

// Tests that foreach over an owned temp (std.args()) doesn't leak.
// The Vec returned by std.args() is hoisted to a temp, used in the loop,
// then __rc_dec'd after the loop.

func main(): i32 {
    var count = 0;
    foreach (const item in std.args()) {
        count = count + 1;
    }
    // std.args() always includes the program name, so count >= 1
    if (count >= 1) {
        return 0;
    }
    return 1;
}

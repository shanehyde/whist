// Expected: PASS: toplevel_const
// Test top-level const declarations

const MAX = 1024;
const PI = 3.14159;
const GREETING = "hello";
const DEBUG = false;
const MAX_I32: i32 = 1024;
private const INTERNAL = 42;
const KEYWORDS: [3]string = ["if", "else", "while"];

test "toplevel_const" {
    var x = MAX;
    var y = PI;
    var s = GREETING;
    var d = DEBUG;
    var m = MAX_I32;
    var i = INTERNAL;
}

// Test: early return from foreach over a temporary expression cleans up the hoisted iterable
// Bug: return inside foreach over e.g. str.split("\n") skipped cleanup of the hoisted Vec.

import std;

// Expected: found

func find_line(text: string, needle: string) -> string {
    foreach (const line in text.split("\n")) {
        if (line.contains(needle)) {
            return line;
        }
    }
    return "";
}

func main() -> i32 {
    var text = "alpha\nbeta\ngamma";
    var result = find_line(text, "beta");
    if (result == "beta") {
        std::println("found");
    }
    return 0;
}

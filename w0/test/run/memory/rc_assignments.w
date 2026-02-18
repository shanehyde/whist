// Expected: PASS: rc_assignments
// Test RC copy semantics (shared reference)

import std;

struct Point {
    x: i64,
    y: i64,
}

struct Line {
    start: Point,
    end: Point,
}

trait Drop {
    func drop(): void;
}

impl Drop for Line {
    func drop() {
        std::print("Deleting line\n");
    }
}

impl Drop for Point {
    func drop() {
        std::print("Deleting point\n");
    }
}

test "rc_assignments" {
    var p = new Point { x: 10, y: 20 };
    var q = new Point { x: 30, y: 40 };
    var z = new Point { x: 50, y: 60 };

    var line1 = new Line { start: p, end: q };

    line1.start = z;
}

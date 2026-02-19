// Expected: PASS: structs
// Expected: PASS: init_basic
// Expected: PASS: init_zero
// Expected: PASS: methods

struct SmallPoint {
    x: i32,
    y: i32,
}

struct Point {
    x: i64,
    y: i64,
}

struct Counter {
    count: i64,
    label: i64,
}

impl Point {
    func init(x: i64, y: i64) {
        self.x = x;
        self.y = y;
    }
}

impl Counter {
    func init(c: i64) {
        self.count = c;
    }
}

// Mutable method
func (Point) move(dx: i64, dy: i64) -> void {
    self.x = self.x + dx;
    self.y = self.y + dy;
}

// Immutable method
func (const Point) sum() -> i64 {
    return self.x + self.y;
}

// Test passing struct reference to function
func move_ref(p: Point) -> void {
    p.move(1, 1);
}

func distance(p: SmallPoint) -> i32 {
    return p.x + p.y;
}

// --- structs ---

test "structs" {
    var p = new SmallPoint {x: 10, y: 30};
    assert(distance(p) == 40);
}

// --- init_basic ---

test "init_basic" {
    var p = new Point(10, 20);
    assert(p.x == 10);
    assert(p.y == 20);
}

// --- init_zero ---

test "init_zero" {
    var c = new Counter(42);
    assert(c.count == 42);
    assert(c.label == 0);
}

// --- methods ---

test "methods" {
    var p = new Point(10, 20);
    p.move(5, 5);
    move_ref(p);     // Calls p.move(1, 1)
    assert(p.sum() == 42);
}

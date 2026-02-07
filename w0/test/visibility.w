// Test visibility modifiers (public keyword)

// Public struct - external linkage
public struct Point {
    x: i64,
    y: i64,
}

// Private struct - file-local (static in C)
struct InternalData {
    value: i64,
}

// Public enum
public enum Color {
    Red,
    Green,
    Blue,
}

// Private enum
enum Status {
    Pending,
    Done,
}

// Public global variable
public var counter: i64 = 0;

// Private global variable (static in C)
var local_counter: i64 = 0;

// Public function - external linkage
public func add(a: i64, b: i64): i64 {
    return a + b;
}

// Private function - file-local (static in C)
private func helper(): void {
    local_counter = local_counter + 1;
}

// Public method on public struct
public func (Point) magnitude_squared(): i64 {
    return self.x * self.x + self.y * self.y;
}

func main(): i32 {
    var p = new Point { x: 3, y: 4 };
    var mag = p.magnitude_squared();

    var data = new InternalData { value: 42 };

    var c: Color = Color::Red;
    var s: Status = Status::Done;

    helper();
    counter = add(1, 2);

    return 0;
}

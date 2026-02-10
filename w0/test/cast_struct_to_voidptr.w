// Test struct reference -> voidptr cast
import std;


struct Box {
    value: i64,
}

func main(): i32 {
    var b = new Box { value: 42 };
    var p: voidptr = b as voidptr;
    if (p == null) {
        return 1;
    }
    std.println($"Box value: {p as u64}");

    return 0;
}

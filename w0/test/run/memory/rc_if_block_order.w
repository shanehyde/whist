// RC RUNTIME TEST: RC cleanup happens at block exit
// Expected rc free order: 1 before 2

struct Box {
    value: i64,
}

func main() -> i32 {
    if (true) {
        var b = new Box { value: 1 };
        if (b.value != 1) {
            return 2;
        }
    }
    var c = new Box { value: 2 };
    if (c.value != 2) {
        return 3;
    }
    return 0;
}

// RC RUNTIME TEST: break should not clean up outer RC vars
// Expected rc free order: 2 before 1

struct Box {
    value: i64,
}

func main() -> i32 {
    var b = new Box { value: 1 };
    var i: i64 = 0;
    while (i < 1) {
        var c = new Box { value: 2 };
        if (c.value != 2) {
            return 2;
        }
        break;
    }
    if (b.value != 1) {
        return 3;
    }
    return 0;
}

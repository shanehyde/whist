// Test: const struct fields can be assigned in init() (multiple const fields)

struct Rect {
    const width: i64,
    const height: i64,
    label: string,
}

impl Rect {
    func init(w: i64, h: i64, l: string) {
        self.width = w;
        self.height = h;
        self.label = l;
    }
}

func main() -> i32 {
    var r = new Rect(10, 20, "box");
    return 0;
}

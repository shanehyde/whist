// Valid: forward references and recursive types should type-check

// Forward reference: Wrapper uses Payload before it's declared
struct Wrapper {
    inner: Payload,
}

struct Payload {
    value: i64,
}

// Self-referential struct
struct TreeNode {
    value: i64,
    left: Option<TreeNode>,
    right: Option<TreeNode>,
}

// Mutually recursive struct + enum
struct Node {
    kind: NodeKind,
    line: i32,
}

enum NodeKind {
    IntLit(i64),
    Binary(string, Node, Node),
}

// Forward reference in enum variant
enum Shape {
    Circle(f64),
    Labeled(Label),
}

struct Label {
    text: string,
}

func main() -> i32 {
    return 0;
}

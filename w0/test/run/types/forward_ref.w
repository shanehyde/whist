// Test forward references and recursive types
// Expected: PASS: forward_ref_struct
// Expected: PASS: self_referential_struct
// Expected: PASS: mutual_recursion
// Expected: PASS: forward_ref_enum_variant
// Expected: PASS: linked_list

// Forward reference: A references B which is declared after A
struct Wrapper {
    inner: Payload,
}

struct Payload {
    value: i64,
}

test "forward_ref_struct" {
    var p = new Payload { value: 99 };
    var w = new Wrapper { inner: p };
    assert(w.inner.value == 99);
}

// Self-referential struct via Option
struct ListNode {
    value: i64,
    next: Option<ListNode>,
}

test "self_referential_struct" {
    var tail = new ListNode { value: 2, next: Option::None };
    var head = new ListNode { value: 1, next: Option::Some(tail) };
    assert(head.value == 1);
    assert(head.next.has_value());
    match (head.next) {
        Some(n) => assert(n.value == 2);
        None => assert(false);
    }
}

// Mutually recursive struct + enum
struct AstNode {
    kind: AstKind,
    line: i32,
}

enum AstKind {
    IntLit(i64),
    Binary(string, AstNode, AstNode),
}

test "mutual_recursion" {
    var left = new AstNode { kind: AstKind::IntLit(1), line: 1 };
    var right = new AstNode { kind: AstKind::IntLit(2), line: 1 };
    var add = new AstNode { kind: AstKind::Binary("+", left, right), line: 1 };
    assert(add.line == 1);
    match (add.kind) {
        Binary(op, l, r) => {
            assert(op == "+");
            match (l.kind) {
                IntLit(v) => assert(v == 1);
                _ => assert(false);
            }
            match (r.kind) {
                IntLit(v) => assert(v == 2);
                _ => assert(false);
            }
        }
        _ => assert(false);
    }
}

// Enum variant referencing a later-declared struct
enum Container {
    Empty,
    HasItem(Item),
}

struct Item {
    name: string,
}

test "forward_ref_enum_variant" {
    var item = new Item { name: "test" };
    var c: Container = Container::HasItem(item);
    match (c) {
        HasItem(i) => assert(i.name == "test");
        Empty => assert(false);
    }
}

// Linked list with traversal
test "linked_list" {
    var c = new ListNode { value: 3, next: Option::None };
    var b = new ListNode { value: 2, next: Option::Some(c) };
    var a = new ListNode { value: 1, next: Option::Some(b) };

    // Walk the list and sum values
    var sum: i64 = a.value;
    match (a.next) {
        Some(n1) => {
            sum = sum + n1.value;
            match (n1.next) {
                Some(n2) => {
                    sum = sum + n2.value;
                }
                None => {}
            }
        }
        None => {}
    }
    assert(sum == 6);
}

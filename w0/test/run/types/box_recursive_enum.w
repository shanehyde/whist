// Expected: PASS: recursive_enum_eval
// Expected: PASS: recursive_enum_nested
// Expected: PASS: recursive_enum_match_leaf

enum Expr {
    IntLit(i64),
    Binary(string, Box<Expr>, Box<Expr>),
}

func eval(e: Expr) -> i64 {
    match (e) {
        IntLit(n) => {
            return n;
        },
        Binary(op, left, right) => {
            var l = eval(left);
            var r = eval(right);
            if (op == "+") {
                return l + r;
            }
            if (op == "*") {
                return l * r;
            }
            return 0;
        },
    }
}

test "recursive_enum_eval" {
    // 2 + 3
    var expr = Expr::Binary("+", new Box<Expr>(Expr::IntLit(2)), new Box<Expr>(Expr::IntLit(3)));
    assert(eval(expr) == 5);
}

test "recursive_enum_nested" {
    // (2 + 3) * 4
    var inner = Expr::Binary("+", new Box<Expr>(Expr::IntLit(2)), new Box<Expr>(Expr::IntLit(3)));
    var expr = Expr::Binary("*", new Box<Expr>(inner), new Box<Expr>(Expr::IntLit(4)));
    assert(eval(expr) == 20);
}

test "recursive_enum_match_leaf" {
    var leaf = Expr::IntLit(42);
    assert(eval(leaf) == 42);
}

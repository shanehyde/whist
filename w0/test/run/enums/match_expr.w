// Expected: PASS: match_expr
// Test match as an expression in multiple expression contexts

enum Animal {
    Dog,
    Cat,
    Other,
}

enum Shape {
    Circle(f64),
    Rect(f64, f64),
    None,
}

func describe(a: Animal): string {
    return match (a) {
        Animal::Dog => "canine",
        Animal::Cat => "feline",
        _ => "other",
    };
}

func area_tag(s: Shape): i64 {
    return match (s) {
        Circle(r) => 1,
        Rect(w, h) => 2,
        None => 3,
    };
}

func add_one(v: i64): i64 {
    return v + 1;
}

test "match_expr" {
    // Match expression in variable initialization
    var label = match (Animal::Cat) {
        Dog => "canine",
        Cat => "feline",
        _ => "other",
    };
    assert(label == "feline");

    // Match expression in return context through helper
    var tag = area_tag(Shape::Rect(3.0, 4.0));
    assert(tag == 2);

    // Match expression as a function argument
    var v: i64 = add_one(match (Shape::Circle(9.0)) {
        Circle(r) => 10,
        Rect(w, h) => 20,
        None => 30,
    });
    assert(v == 11);

    assert(describe(Animal::Dog) == "canine");
}

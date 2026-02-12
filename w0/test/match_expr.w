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

func main(): i32 {
    // Match expression in variable initialization
    var label = match (Animal::Cat) {
        Dog => "canine",
        Cat => "feline",
        _ => "other",
    };
    if (label != "feline") {
        return 1;
    }

    // Match expression in return context through helper
    var tag = area_tag(Shape::Rect(3.0, 4.0));
    if (tag != 2) {
        return 2;
    }

    // Match expression as a function argument
    var v: i64 = add_one(match (Shape::Circle(9.0)) {
        Circle(r) => 10,
        Rect(w, h) => 20,
        None => 30,
    });
    if (v != 11) {
        return 3;
    }

    if (describe(Animal::Dog) != "canine") {
        return 4;
    }

    return 0;
}

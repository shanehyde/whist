// RC RUNTIME TEST: pop on Vec of structs — ownership transfer, no double-free

struct Item {
    value: i64,
}

func main(): i32 {
    var items = new Vec<Item>{};
    items.push(new Item { value: 10 });
    items.push(new Item { value: 20 });
    items.push(new Item { value: 30 });

    // pop() transfers ownership — no __rc_inc needed
    var p = items.pop();
    match (p) {
        Some(item) => {
            if (item.value != 30) { return 1; }
        },
        None => { return 2; },
    }

    // Vec now has 2 elements
    if (items.count != 2) { return 3; }

    // Pop remaining elements
    var p2 = items.pop();
    match (p2) {
        Some(item) => {
            if (item.value != 20) { return 4; }
        },
        None => { return 5; },
    }

    var p3 = items.pop();
    match (p3) {
        Some(item) => {
            if (item.value != 10) { return 6; }
        },
        None => { return 7; },
    }

    // Empty vec pop returns None
    var p4 = items.pop();
    match (p4) {
        Some(item) => { return 8; },
        None => {},
    }

    return 0;
}

// RC RUNTIME TEST: first/last on Vec of structs — no double-free or leaks

struct Item {
    value: i64,
}

func main(): i32 {
    var items = new Vec<Item>{};
    items.push(new Item { value: 10 });
    items.push(new Item { value: 20 });
    items.push(new Item { value: 30 });

    // first() returns a copy with incremented RC
    var f = items.first();
    match (f) {
        Some(item) => {
            if (item.value != 10) { return 1; }
        },
        None => { return 2; },
    }

    // last() returns a copy with incremented RC
    var l = items.last();
    match (l) {
        Some(item) => {
            if (item.value != 30) { return 3; }
        },
        None => { return 4; },
    }

    // Vec still intact after first/last
    if (items.count != 3) { return 5; }
    if (items[0].value != 10) { return 6; }
    if (items[2].value != 30) { return 7; }

    // Empty vec of structs
    var empty = new Vec<Item>{};
    var ef = empty.first();
    match (ef) {
        Some(item) => { return 8; },
        None => {},
    }

    return 0;
}

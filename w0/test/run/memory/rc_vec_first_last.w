// Expected: PASS: rc_vec_first_last
// RC RUNTIME TEST: first/last on Vec of structs — no double-free or leaks

struct Item {
    value: i64,
}

test "rc_vec_first_last" {
    var items = new Vec<Item>{};
    items.push(new Item { value: 10 });
    items.push(new Item { value: 20 });
    items.push(new Item { value: 30 });

    // first() returns a copy with incremented RC
    var f = items.first();
    match (f) {
        Some(item) => {
            assert(item.value == 10);
        },
        None => { assert(false); },
    }

    // last() returns a copy with incremented RC
    var l = items.last();
    match (l) {
        Some(item) => {
            assert(item.value == 30);
        },
        None => { assert(false); },
    }

    // Vec still intact after first/last
    assert(items.count == 3);
    assert(items[0].value == 10);
    assert(items[2].value == 30);

    // Empty vec of structs
    var empty = new Vec<Item>{};
    var ef = empty.first();
    match (ef) {
        Some(item) => { assert(false); },
        None => {},
    }
}

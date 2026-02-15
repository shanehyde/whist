// Expected: PASS: rc_vec_basic
// Expected: PASS: rc_vec_first_last
// Expected: PASS: rc_vec_pop

struct Item {
    value: i64,
}

// RC RUNTIME TEST: Vec cleanup frees data and elements
test "rc_vec_basic" {
    var items = new Vec<Item>{};
    items.push(new Item { value: 1 });
    items.push(new Item { value: 2 });
    items.push(new Item { value: 3 });

    assert(items.count == 3);

    var first: Item = items[0];
    assert(first.value == 1);

    var last: Item = items[2];
    assert(last.value == 3);

    // Vec goes out of scope here — should free Vec + all Item elements
}

// RC RUNTIME TEST: first/last on Vec of structs — no double-free or leaks
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

// RC RUNTIME TEST: pop on Vec of structs — ownership transfer, no double-free
test "rc_vec_pop" {
    var items = new Vec<Item>{};
    items.push(new Item { value: 10 });
    items.push(new Item { value: 20 });
    items.push(new Item { value: 30 });

    // pop() transfers ownership — no __rc_inc needed
    var p = items.pop();
    match (p) {
        Some(item) => {
            assert(item.value == 30);
        },
        None => { assert(false); },
    }

    // Vec now has 2 elements
    assert(items.count == 2);

    // Pop remaining elements
    var p2 = items.pop();
    match (p2) {
        Some(item) => {
            assert(item.value == 20);
        },
        None => { assert(false); },
    }

    var p3 = items.pop();
    match (p3) {
        Some(item) => {
            assert(item.value == 10);
        },
        None => { assert(false); },
    }

    // Empty vec pop returns None
    var p4 = items.pop();
    match (p4) {
        Some(item) => { assert(false); },
        None => {},
    }
}

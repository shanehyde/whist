// Expected: PASS: rc_vec_pop
// RC RUNTIME TEST: pop on Vec of structs — ownership transfer, no double-free

struct Item {
    value: i64,
}

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

// Expected: PASS: rc_vec_basic
// RC RUNTIME TEST: Vec cleanup frees data and elements

struct Item {
    value: i64,
}

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

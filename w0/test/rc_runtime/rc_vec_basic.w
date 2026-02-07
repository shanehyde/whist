// RC RUNTIME TEST: Vec cleanup frees data and elements

struct Item {
    value: i64,
}

func main(): i32 {
    var items = new Vec<Item>{};
    items.push(new Item { value: 1 });
    items.push(new Item { value: 2 });
    items.push(new Item { value: 3 });

    if (items.count != 3) {
        return 1;
    }

    var first: Item = items[0];
    if (first.value != 1) {
        return 2;
    }

    var last: Item = items[2];
    if (last.value != 3) {
        return 3;
    }

    // Vec goes out of scope here — should free Vec + all Item elements
    return 0;
}

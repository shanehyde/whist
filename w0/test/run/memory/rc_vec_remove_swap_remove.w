// RC RUNTIME TEST: remove/swap_remove return owned elements without early decref
// Expected rc free order: 1 before 3

struct Item {
    value: i64,
}

func main(): i32 {
    var items = new Vec<Item>{};
    items.push(new Item { value: 10 });
    items.push(new Item { value: 20 });
    items.push(new Item { value: 30 });

    var removed = items.remove(1);
    if (removed.value != 20) {
        return 1;
    }
    if (items.count != 2 || items[0].value != 10 || items[1].value != 30) {
        return 2;
    }

    var swapped = items.swap_remove(0);
    if (swapped.value != 10) {
        return 3;
    }
    if (items.count != 1 || items[0].value != 30) {
        return 4;
    }

    var tail = items.remove(0);
    if (tail.value != 30) {
        return 5;
    }
    if (items.count != 0) {
        return 6;
    }

    return 0;
}

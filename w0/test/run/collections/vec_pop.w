// Test Vec pop operation — returns Option<T>

func main(): i32 {
    var nums = new Vec<i64>{10, 20, 30};

    // Pop returns Some(last_element)
    var last = nums.pop();
    match (last) {
        Some(v) => {
            if (v != 30) { return 1; }
        },
        None => { return 2; },
    }

    if (nums.count != 2) {
        return 3;
    }

    // Pop second element
    var second = nums.pop();
    match (second) {
        Some(v) => {
            if (v != 20) { return 4; }
        },
        None => { return 5; },
    }

    // Pop first element
    var first = nums.pop();
    match (first) {
        Some(v) => {
            if (v != 10) { return 6; }
        },
        None => { return 7; },
    }

    if (nums.count != 0) {
        return 8;
    }

    // Pop from empty Vec returns None
    var empty_pop = nums.pop();
    match (empty_pop) {
        Some(v) => { return 9; },
        None => {},
    }

    return 0;
}

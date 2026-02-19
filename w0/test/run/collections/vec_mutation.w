// Expected: PASS: vec_eq
// Expected: PASS: vec_insert_remove_swap_remove

// Shared definitions

struct Point { x: i64, y: i64 }

impl Eq for Point {
    func eq(other: Point) -> bool {
        return self.x == other.x && self.y == other.y;
    }
}

// Tests

test "vec_eq" {
    // Equal i64 vecs
    var a = new Vec<i64>{1, 2, 3};
    var b = new Vec<i64>{1, 2, 3};
    assert(a == b);

    // Unequal elements
    var c = new Vec<i64>{1, 2, 4};
    assert(a != c);

    // Different lengths
    var d = new Vec<i64>{1, 2};
    assert(a != d);

    // Empty vecs
    var e = new Vec<i64>{};
    var f = new Vec<i64>{};
    assert(e == f);
    assert(e != a);

    // String vecs
    var s1 = new Vec<string>{"hello", "world"};
    var s2 = new Vec<string>{"hello", "world"};
    var s3 = new Vec<string>{"hello", "whist"};
    assert(s1 == s2);
    assert(s1 != s3);

    // Struct vecs (with Eq impl)
    var p1 = new Vec<Point>{new Point{x: 1, y: 2}, new Point{x: 3, y: 4}};
    var p2 = new Vec<Point>{new Point{x: 1, y: 2}, new Point{x: 3, y: 4}};
    var p3 = new Vec<Point>{new Point{x: 1, y: 2}, new Point{x: 5, y: 6}};
    assert(p1 == p2);
    assert(p1 != p3);

    // != operator
    assert(!(a != b));
    assert(!(a == c));
}

test "vec_insert_remove_swap_remove" {
    var nums = new Vec<i64>{1, 2, 3};

    nums.insert(0, 10);
    nums.insert(2, 20);
    nums.insert(nums.count, 30);

    assert(nums.count == 6);
    assert(nums[0] == 10 && nums[1] == 1 && nums[2] == 20 && nums[3] == 2 && nums[4] == 3 &&
        nums[5] == 30);

    var removed_start = nums.remove(0);
    assert(removed_start == 10);

    var removed_mid = nums.remove(1);
    assert(removed_mid == 20);

    var removed_end = nums.remove(nums.count - 1);
    assert(removed_end == 30);

    assert(nums.count == 3 && nums[0] == 1 && nums[1] == 2 && nums[2] == 3);

    var swapped_start = nums.swap_remove(0);
    assert(swapped_start == 1);
    assert(nums.count == 2 && nums[0] == 3 && nums[1] == 2);

    var swapped_end = nums.swap_remove(1);
    assert(swapped_end == 2);
    assert(nums.count == 1 && nums[0] == 3);

    var swapped_last = nums.swap_remove(0);
    assert(swapped_last == 3 && nums.count == 0);
}

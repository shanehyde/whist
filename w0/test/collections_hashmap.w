import collections;
import std;

func test_string_keys(): i32 {
    var m = new HashMap<string, i32>{
        buckets: new Vec<HashEntry<string, i32>>{},
        count: 0, capacity: 0,
    };
    m.init(16);

    // Test set and get
    m.set("hello", 1);
    m.set("world", 2);
    m.set("foo", 3);

    if (m.count != 3) {
        std.print("FAIL: expected count 3\n");
        return 1;
    }

    var v1 = m.get("hello");
    match (v1) {
        Some(val) => {
            if (val != 1) {
                std.print("FAIL: hello should be 1\n");
                return 2;
            }
        },
        None => {
            std.print("FAIL: hello not found\n");
            return 3;
        },
    }

    var v2 = m.get("world");
    match (v2) {
        Some(val) => {
            if (val != 2) {
                std.print("FAIL: world should be 2\n");
                return 4;
            }
        },
        None => {
            std.print("FAIL: world not found\n");
            return 5;
        },
    }

    // Test has
    if (!m.has("foo")) {
        std.print("FAIL: should have foo\n");
        return 6;
    }
    if (m.has("bar")) {
        std.print("FAIL: should not have bar\n");
        return 7;
    }

    // Test get on non-existent key
    var v3 = m.get("missing");
    match (v3) {
        Some(val) => {
            std.print(std.format("FAIL: missing should be None, got %d\n", val));
            return 8;
        },
        None => {},
    }

    // Test overwrite existing key
    m.set("hello", 99);
    if (m.count != 3) {
        std.print("FAIL: count should still be 3 after overwrite\n");
        return 9;
    }
    var v4 = m.get("hello");
    match (v4) {
        Some(val) => {
            if (val != 99) {
                std.print("FAIL: hello should be 99 after overwrite\n");
                return 10;
            }
        },
        None => {
            std.print("FAIL: hello not found after overwrite\n");
            return 11;
        },
    }

    // Test delete
    var deleted = m.delete("world");
    if (!deleted) {
        std.print("FAIL: delete world should return true\n");
        return 12;
    }
    if (m.count != 2) {
        std.print("FAIL: count should be 2 after delete\n");
        return 13;
    }
    if (m.has("world")) {
        std.print("FAIL: should not have world after delete\n");
        return 14;
    }

    // Test delete non-existent key
    var deleted2 = m.delete("nonexistent");
    if (deleted2) {
        std.print("FAIL: delete nonexistent should return false\n");
        return 15;
    }

    // Test keys
    var k = m.keys();
    if (k.count != 2) {
        std.print("FAIL: keys should have 2 entries\n");
        return 16;
    }

    std.print("string keys: PASS\n");
    return 0;
}

func test_i64_keys(): i32 {
    var m = new HashMap<i64, string>{
        buckets: new Vec<HashEntry<i64, string>>{},
        count: 0, capacity: 0,
    };
    m.init(8);

    m.set(100, "hundred");
    m.set(200, "two hundred");
    m.set(300, "three hundred");

    if (m.count != 3) {
        std.print("FAIL: i64 count should be 3\n");
        return 1;
    }

    var v1 = m.get(100);
    match (v1) {
        Some(val) => {
            if (val != "hundred") {
                std.print("FAIL: 100 should be hundred\n");
                return 2;
            }
        },
        None => {
            std.print("FAIL: 100 not found\n");
            return 3;
        },
    }

    if (!m.has(200)) {
        std.print("FAIL: should have 200\n");
        return 4;
    }

    m.delete(200);
    if (m.has(200)) {
        std.print("FAIL: should not have 200 after delete\n");
        return 5;
    }

    // Test overwrite
    m.set(100, "one hundred");
    var v2 = m.get(100);
    match (v2) {
        Some(val) => {
            if (val != "one hundred") {
                std.print("FAIL: 100 should be one hundred\n");
                return 6;
            }
        },
        None => {
            std.print("FAIL: 100 not found after overwrite\n");
            return 7;
        },
    }

    std.print("i64 keys: PASS\n");
    return 0;
}

func main(): i32 {
    var r1 = test_string_keys();
    if (r1 != 0) {
        return r1;
    }

    var r2 = test_i64_keys();
    if (r2 != 0) {
        return 100 + r2;
    }

    std.print("All HashMap tests passed!\n");
    return 0;
}

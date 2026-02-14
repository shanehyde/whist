// Expected: PASS: collections_hashmap
import collections;

test "collections_hashmap" {
    // Test string keys
    var m = new HashMap<string, i32>{
        buckets: new Vec<HashEntry<string, i32>>{},
        count: 0, capacity: 0,
    };
    m.init(16);

    // Test set and get
    m.set("hello", 1);
    m.set("world", 2);
    m.set("foo", 3);

    assert(m.count == 3);

    var v1 = m.get("hello");
    match (v1) {
        Some(val) => {
            assert(val == 1);
        },
        None => {
            assert(false);
        },
    }

    var v2 = m.get("world");
    match (v2) {
        Some(val) => {
            assert(val == 2);
        },
        None => {
            assert(false);
        },
    }

    // Test has
    assert(m.has("foo"));
    assert(!m.has("bar"));

    // Test get on non-existent key
    var v3 = m.get("missing");
    match (v3) {
        Some(val) => {
            assert(false);
        },
        None => {},
    }

    // Test overwrite existing key
    m.set("hello", 99);
    assert(m.count == 3);
    var v4 = m.get("hello");
    match (v4) {
        Some(val) => {
            assert(val == 99);
        },
        None => {
            assert(false);
        },
    }

    // Test delete
    var deleted = m.delete("world");
    assert(deleted);
    assert(m.count == 2);
    assert(!m.has("world"));

    // Test delete non-existent key
    var deleted2 = m.delete("nonexistent");
    assert(!deleted2);

    // Test keys
    var k = m.keys();
    assert(k.count == 2);

    // Test i64 keys
    var m2 = new HashMap<i64, string>{
        buckets: new Vec<HashEntry<i64, string>>{},
        count: 0, capacity: 0,
    };
    m2.init(8);

    m2.set(100, "hundred");
    m2.set(200, "two hundred");
    m2.set(300, "three hundred");

    assert(m2.count == 3);

    var v5 = m2.get(100);
    match (v5) {
        Some(val) => {
            assert(val == "hundred");
        },
        None => {
            assert(false);
        },
    }

    assert(m2.has(200));

    m2.delete(200);
    assert(!m2.has(200));

    // Test overwrite
    m2.set(100, "one hundred");
    var v6 = m2.get(100);
    match (v6) {
        Some(val) => {
            assert(val == "one hundred");
        },
        None => {
            assert(false);
        },
    }
}

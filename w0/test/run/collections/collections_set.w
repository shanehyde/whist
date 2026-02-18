// Expected: PASS: collections_set
import collections;

test "collections_set" {
    // Test string set
    var s = new Set<string>(16);

    // Test insert and contains
    s.insert("hello");
    s.insert("world");
    s.insert("foo");

    assert(s.count == 3);
    assert(s.contains("hello"));
    assert(s.contains("world"));
    assert(s.contains("foo"));
    assert(!s.contains("bar"));

    // Test duplicate insert (no-op)
    s.insert("hello");
    assert(s.count == 3);

    // Test remove
    var removed = s.remove("world");
    assert(removed);
    assert(s.count == 2);
    assert(!s.contains("world"));

    // Test remove non-existent
    var removed2 = s.remove("nonexistent");
    assert(!removed2);
    assert(s.count == 2);

    // Test values
    var vals = s.values();
    assert(vals.count == 2);

    // Test i64 set
    var s2 = new Set<i64>(8);

    s2.insert(100);
    s2.insert(200);
    s2.insert(300);

    assert(s2.count == 3);
    assert(s2.contains(100));
    assert(s2.contains(200));
    assert(s2.contains(300));
    assert(!s2.contains(400));

    // Duplicate
    s2.insert(200);
    assert(s2.count == 3);

    // Remove
    s2.remove(200);
    assert(!s2.contains(200));
    assert(s2.count == 2);

    // Test hash collisions via small capacity
    var s3 = new Set<i64>(2);

    s3.insert(1);
    s3.insert(2);
    s3.insert(3);
    s3.insert(4);

    assert(s3.count == 4);
    assert(s3.contains(1));
    assert(s3.contains(2));
    assert(s3.contains(3));
    assert(s3.contains(4));

    // Remove from collision chain
    s3.remove(3);
    assert(s3.count == 3);
    assert(!s3.contains(3));
    assert(s3.contains(1));
    assert(s3.contains(4));

    var vals3 = s3.values();
    assert(vals3.count == 3);
}

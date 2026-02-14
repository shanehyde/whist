// Expected: PASS: hash_table
import std;

enum Option<V> {
    Some(V),
    None,
}

trait Drop {
    func drop(): void;
}

trait Hashable {
    const func hash(): i32;
}

impl Hashable for i32 {
    const func hash(): i32 {
        return self;
    }
}

struct HashEntry<K: Hashable, V> {
    next: HashEntry<K,V>,
    value: V,
    key: K,
}

struct HashTable<K, V>  {
    buckets: Vec<HashEntry<K, V>> ,
    size: u32,
}

func (HashTable<K,V>) init(): void {
    self.buckets.clear();
    self.size = 2;
    foreach (const i in 0..self.size) {
        self.buckets.push(null);
    }
}

func (HashTable<K,V>) insert(key: K, value: V): void {
    var index: u32 = key.hash() % self.size;
    var entry: HashEntry<K,V> = new HashEntry<K,V>{next: null, value: value, key: key};
    entry.next = self.buckets[index];
    self.buckets[index] = entry;
}

func (HashTable<K,V>) get(key: K): Option<V> {
    var index: u32 = key.hash() % self.size;
    var entry: HashEntry<K,V> = self.buckets[index];
    while (entry != null) {
        if (entry.key == key) {
            return Option::Some(entry.value);
        }
        entry = entry.next;
    }
    return Option::None;
}

type HashMap = HashTable<i32, i32>;
type HashMapEntry = HashEntry<i32, i32>;

test "hash_table" {
    var h: HashMap = new HashMap{
        size: 0,
        buckets: new Vec<HashMapEntry>{},
    };

    h.init();
    h.insert(1, 42);
    h.insert(2, 84);
    h.insert(3, 126);

    var v1 = h.get(3);
    match (v1) {
        Some(val) => {
            assert(val == 126);
        },
        None => {
            assert(false);
        },
    }

    var v2 = h.get(1);
    match (v2) {
        Some(val) => {
            assert(val == 42);
        },
        None => {
            assert(false);
        },
    }

    var v3 = h.get(2);
    match (v3) {
        Some(val) => {
            assert(val == 84);
        },
        None => {
            assert(false);
        },
    }
}

// enum Option<V> {
//     Some(V),
//     None,
// }
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

// impl Drop for HashEntry<K,V> {
//     func drop(): void {
//         std.print("Dropping HashEntry\n");
//         self.next = null;
//     }
// }

struct HashTable<K, V>  {
    buckets: Vec<HashEntry<K, V>> ,
    size: u32,
}

func (HashTable<K,V>) init(): void {
    // self.buckets = new Vec<HashEntry<V>>{};
    self.buckets.clear();
    self.size = 2;
    foreach (const i in 0..self.size) {
        self.buckets.push(null);//new HashEntry<K, V>{next: null, value: 0, key: 0});
    }
    // Implementation of insert method
}


func (HashTable<K,V>) insert(key: K, value: V): void {
    var index: u32 = key.hash() % self.size;
    std.print(std.format("Inserting key %d at index %d\n", key, index));
    var entry: HashEntry<K,V> = new HashEntry<K,V>{next: null, value: value, key: key};
    entry.next = self.buckets[index];
    self.buckets[index] = entry;
}

func (HashTable<K,V>) get(key: K): Option<V> {
    var index: u32 = key.hash() % self.size;
    var entry: HashEntry<K,V> = self.buckets[index];
    std.print(std.format("Looking for key %d at index %d\n", key, index));
    std.print("Traversing bucket linked list\n");
    std.print("Bucket contents:\n");
    var temp: HashEntry<K,V> = entry;
    while (temp != null) {
        std.print(std.format("Entry key: %d\n", temp.key));
        temp = temp.next;
    }
    while (entry != null) {
        if (entry.key == key) {
            return Option::Some(entry.value);
        }
        entry = entry.next;
    }
    return Option::None;
}

// impl Drop for HashTable<K, V> {
//     func drop(): void {
//         std.print("Dropping HashTable\n");
//         // self.buckets.clear();
//     }
// }

type HashMap = HashTable<i32, i32>;
type HashMapEntry = HashEntry<i32, i32>;

func main(): i32 {

    std.print("Testing HashTable\n");

    var h: HashMap = new HashMap{
        size: 0,
        buckets: new Vec<HashMapEntry>{},
    };

    h.init();
    h.insert(1, 42);
    h.insert(2, 84);
    h.insert(3, 126);
    std.print("Inserted entries into HashTable\n");

    var v1 = h.get(3);
    match (v1) {
        Some(val) => {std.print(std.format("Value for key 3: %d\n", val));},
        None => {std.print("Key 3 not found\n");},
    }


    return 0;
}
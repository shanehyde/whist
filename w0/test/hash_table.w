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
    const func hash(): u32;
}

struct HashEntry<K,V> {
    next: HashEntry<K,V>,
    value: V,
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
    foreach (const i in 0..1) {
        self.buckets.push(new HashEntry<K, V>{next: null, value: 0});
    }
    // Implementation of insert method
}

// func (i32) hash(): u32 {
//     return self ;
// }

func (HashTable<K,V>) insert(key: K, value: V): void {
    var index: u32 = key % self.size;//.hash() % self.size;
    var entry: HashEntry<K,V> = new HashEntry<K,V>{next: null, value: value};
    entry.next = self.buckets[index];
    self.buckets[index] = entry;
}

func (HashTable<K,V>) get(key: K): Option<V> {
    var index: u32 = key % self.size;//.hash() % self.size;
    var entry: HashEntry<K,V> = self.buckets[index];
    while (entry != null) {
        if (entry.value == key) {
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

type HashMap = HashTable<u32, i32>;
type HashMapEntry = HashEntry<u32, i32>;

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


    return 0;
}
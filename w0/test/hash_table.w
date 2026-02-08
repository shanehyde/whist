// enum Option<V> {
//     Some(V),
//     None,
// }
import std;

trait Drop {
    func drop(): void;
}

struct HashEntry<K,V> {
    next: HashEntry<K,V>,
    value: V,
}

impl Drop for HashEntry<K,V> {
    func drop(): void {
        std.print("Dropping HashEntry\n");
        // self.next = null;
    }
}

// struct HashTable<V>  {
//     buckets: Vec<HashEntry<V>> ,
//     size: u32,
// }

struct HashTable<K, V>  {
    buckets: Vec<HashEntry<K, V>> ,
    size: u32,
}

func (HashTable<K,V>) init(): void {
    // self.buckets = new Vec<HashEntry<V>>{};
    self.buckets.clear();
    self.size = 0;
    foreach (const i in 0..16) {
        self.buckets.push(new HashEntry<K, V>{next: null, value: 0});
    }
    // Implementation of insert method
}

impl Drop for HashTable<K, V> {
    func drop(): void {
        std.print("Dropping HashTable\n");
        self.buckets.clear();
    }
}

type HashMap = HashTable<string, i32>;
type HashMapEntry = HashEntry<string, i32>;

func main(): i32 {

    std.print("Testing HashTable\n");

    var h: HashMap = new HashMap{
        size: 0,
        buckets: new Vec<HashMapEntry>{},
    };

    h.init();

    return 0;
}
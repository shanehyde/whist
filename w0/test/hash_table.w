// enum Option<V> {
//     Some(V),
//     None,
// }
import std;

trait Drop {
    func drop(): void;
}

struct HashEntry<V> {
    next: HashEntry<V>,
    value: V,
}

impl Drop for HashEntry<V> {
    func drop(): void {
        std.print("Dropping HashEntry\n");
        // self.next = null;
    }
}

struct HashTable<V>  {
    buckets: Vec<HashEntry<V>> ,
    size: u32,
}

func (HashTable<V>) init(): void {
    // self.buckets = new Vec<HashEntry<V>>{};
    self.buckets.clear();
    self.size = 0;
    foreach (const i in 0..16) {
        self.buckets.push(new HashEntry<V> {next: null, value: 0});
    }
    // Implementation of insert method
}

impl Drop for HashTable<V> {
    func drop(): void {
        std.print("Dropping HashTable\n");
        self.buckets.clear();
    }
}

// type HashMap = HashTable<string, i32>;

func main(): i32 {

    std.print("Testing HashTable\n");

    var h: HashTable<i32> = new HashTable<i32>{
        size: 0,
        buckets: new Vec<HashEntry<i32>>{},
    };

    h.init();

    return 0;
}
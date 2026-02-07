// enum Option<V> {
//     Some(V),
//     None,
// }

struct HashEntry<V> {
    next: HashEntry<V>,
    value: V,
}

struct HashTable<V>  {
    buckets: Vec<HashEntry<V>> ,
    size: u32,
}

func (HashTable<V>) init(): void {
    self.buckets = new Vec<HashEntry<V>>{};
    self.size = 0;
    foreach (const i in 0..16) {
        self.buckets.push(new HashEntry<V> {next: null, value: 0});
    }
    // Implementation of insert method
}

// type HashMap = HashTable<string, i32>;

func main(): i32 {

    var h: HashTable<i32> = new HashTable<i32>{
        size: 0,
        buckets: new Vec<HashEntry<i32>>{},
    };

    h.init();

    return 0;
}
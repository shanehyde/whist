// Collections library for Whist
//
// Usage:
//   import collections;
//   var m = new HashMap<string, i32>{
//       buckets: new Vec<HashEntry<string, i32>>{},
//       count: 0, capacity: 0,
//   };
//   m.init(16);
//   m.set("key", 42);

enum Option<V> {
    None,
    Some(V),
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

impl Hashable for i64 {
    const func hash(): i32 {
        return self;
    }
}

impl Hashable for bool {
    const func hash(): i32 {
        if (self) {
            return 1;
        }
        return 0;
    }
}

impl Hashable for string {
    const func hash(): i32 {
        var h: i32 = 0;
        var x: string = self;
        foreach (const c in x) {
            h = h * 31 + c;
        }
        return h;
    }
}

struct HashEntry<K: Hashable, V> {
    next: HashEntry<K, V>,
    key: K,
    value: V,
}

struct HashMap<K: Hashable, V> {
    buckets: Vec<HashEntry<K, V>>,
    count: i64,
    capacity: i64,
}

func (HashMap<K, V>) init(cap: i64): void {
    self.buckets.clear();
    self.count = 0;
    self.capacity = cap;
    foreach (const i in 0..cap) {
        self.buckets.push(null);
    }
}

func (HashMap<K, V>) set(key: K, value: V): void {
    var h: i32 = key.hash();
    if (h < 0) {
        h = -h;
    }
    var index: i64 = h % self.capacity;

    // Check if key already exists, update value if so
    var entry: HashEntry<K, V> = self.buckets[index];
    while (entry != null) {
        if (entry.key == key) {
            entry.value = value;
            return;
        }
        entry = entry.next;
    }

    // Key not found, insert new entry at head of bucket
    var new_entry: HashEntry<K, V> = new HashEntry<K, V>{
        next: self.buckets[index],
        key: key,
        value: value,
    };
    self.buckets[index] = new_entry;
    self.count = self.count + 1;
}

func (HashMap<K, V>) get(key: K): Option<V> {
    var h: i32 = key.hash();
    if (h < 0) {
        h = -h;
    }
    var index: i64 = h % self.capacity;

    var entry: HashEntry<K, V> = self.buckets[index];
    while (entry != null) {
        if (entry.key == key) {
            return Option::Some(entry.value);
        }
        entry = entry.next;
    }
    return Option::None;
}

func (HashMap<K, V>) has(key: K): bool {
    var h: i32 = key.hash();
    if (h < 0) {
        h = -h;
    }
    var index: i64 = h % self.capacity;

    var entry: HashEntry<K, V> = self.buckets[index];
    while (entry != null) {
        if (entry.key == key) {
            return true;
        }
        entry = entry.next;
    }
    return false;
}

func (HashMap<K, V>) delete(key: K): bool {
    var h: i32 = key.hash();
    if (h < 0) {
        h = -h;
    }
    var index: i64 = h % self.capacity;

    var entry: HashEntry<K, V> = self.buckets[index];
    var prev: HashEntry<K, V> = null;

    while (entry != null) {
        if (entry.key == key) {
            if (prev == null) {
                self.buckets[index] = entry.next;
            } else {
                prev.next = entry.next;
            }
            self.count = self.count - 1;
            return true;
        }
        prev = entry;
        entry = entry.next;
    }
    return false;
}

func (HashMap<K, V>) keys(): Vec<K> {
    var result = new Vec<K>{};
    foreach (const i in 0..self.capacity) {
        var entry: HashEntry<K, V> = self.buckets[i];
        while (entry != null) {
            result.push(entry.key);
            entry = entry.next;
        }
    }
    return result;
}

// --- Set ---

struct SetEntry<T: Hashable> {
    next: SetEntry<T>,
    key: T,
}

struct Set<T: Hashable> {
    buckets: Vec<SetEntry<T>>,
    count: i64,
    capacity: i64,
}

func (Set<T>) init(cap: i64): void {
    self.buckets.clear();
    self.count = 0;
    self.capacity = cap;
    foreach (const i in 0..cap) {
        self.buckets.push(null);
    }
}

func (Set<T>) insert(value: T): void {
    var h: i32 = value.hash();
    if (h < 0) {
        h = -h;
    }
    var index: i64 = h % self.capacity;

    // Check if value already exists
    var entry: SetEntry<T> = self.buckets[index];
    while (entry != null) {
        if (entry.key == value) {
            return;
        }
        entry = entry.next;
    }

    // Not found, insert at head of bucket
    var new_entry: SetEntry<T> = new SetEntry<T>{
        next: self.buckets[index],
        key: value,
    };
    self.buckets[index] = new_entry;
    self.count = self.count + 1;
}

func (Set<T>) contains(value: T): bool {
    var h: i32 = value.hash();
    if (h < 0) {
        h = -h;
    }
    var index: i64 = h % self.capacity;

    var entry: SetEntry<T> = self.buckets[index];
    while (entry != null) {
        if (entry.key == value) {
            return true;
        }
        entry = entry.next;
    }
    return false;
}

func (Set<T>) remove(value: T): bool {
    var h: i32 = value.hash();
    if (h < 0) {
        h = -h;
    }
    var index: i64 = h % self.capacity;

    var entry: SetEntry<T> = self.buckets[index];
    var prev: SetEntry<T> = null;

    while (entry != null) {
        if (entry.key == value) {
            if (prev == null) {
                self.buckets[index] = entry.next;
            } else {
                prev.next = entry.next;
            }
            self.count = self.count - 1;
            return true;
        }
        prev = entry;
        entry = entry.next;
    }
    return false;
}

func (Set<T>) values(): Vec<T> {
    var result = new Vec<T>{};
    foreach (const i in 0..self.capacity) {
        var entry: SetEntry<T> = self.buckets[i];
        while (entry != null) {
            result.push(entry.key);
            entry = entry.next;
        }
    }
    return result;
}

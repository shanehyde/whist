# Standard Library: Collections

Data structures for storing and manipulating groups of values.

## Overview

| Type | Description | Use Case |
|------|-------------|----------|
| `Vec<T>` | Growable array | General-purpose list |
| `HashMap<K, V>` | Hash-based key-value store | Fast lookup by key |
| `HashSet<T>` | Unique elements | Membership testing |
| `LinkedList<T>` | Doubly-linked list | Frequent insert/remove |
| `Queue<T>` | FIFO collection | Task scheduling |
| `Stack<T>` | LIFO collection | Undo, parsing |
| `BTreeMap<K, V>` | Sorted key-value store | Ordered iteration |
| `BTreeSet<T>` | Sorted unique elements | Ordered membership |

## Vec<T>

Growable, contiguous array.

### API

```whist
struct Vec<T> {
    // Internal: data pointer, length, capacity
}

impl<T> Vec<T> {
    // Constructors
    func new(): Vec<T>;
    func with_capacity(cap: i64): Vec<T>;
    func from_array(arr: [T]): Vec<T>;
    func from_span(span: Span<T>): Vec<T>;

    // Access
    func (Vec<T>) get(index: i64): ?T;
    func (Vec<T>) get_unchecked(index: i64): T;
    func (Vec<T>) first(): ?T;
    func (Vec<T>) last(): ?T;
    func (Vec<T>) as_span(): Span<T>;

    // Modification
    func (Vec<T>) push(value: T): void;
    func (Vec<T>) pop(): ?T;
    func (Vec<T>) insert(index: i64, value: T): void;
    func (Vec<T>) remove(index: i64): T;
    func (Vec<T>) swap_remove(index: i64): T;  // O(1) remove
    func (Vec<T>) clear(): void;
    func (Vec<T>) truncate(len: i64): void;

    // Size
    func (Vec<T>) len(): i64;
    func (Vec<T>) capacity(): i64;
    func (Vec<T>) is_empty(): bool;
    func (Vec<T>) reserve(additional: i64): void;
    func (Vec<T>) shrink_to_fit(): void;

    // Iteration
    func (Vec<T>) iter(): Iterator<T>;
    func (Vec<T>) iter_mut(): MutIterator<T>;

    // Functional
    func (Vec<T>) map<U>(f: func(T): U): Vec<U>;
    func (Vec<T>) filter(pred: func(T): bool): Vec<T>;
    func (Vec<T>) fold<A>(init: A, f: func(A, T): A): A;
    func (Vec<T>) find(pred: func(T): bool): ?T;
    func (Vec<T>) any(pred: func(T): bool): bool;
    func (Vec<T>) all(pred: func(T): bool): bool;

    // Sorting (requires T: Ord)
    func (Vec<T>) sort(): void;
    func (Vec<T>) sort_by(cmp: func(T, T): Ordering): void;

    // Misc
    func (Vec<T>) reverse(): void;
    func (Vec<T>) contains(value: T): bool;  // requires T: Eq
    func (Vec<T>) dedup(): void;             // requires T: Eq
    func (Vec<T>) clone(): Vec<T>;           // requires T: Clone
}

// Index operator
func (Vec<T>) [](index: i64): T;
func (Vec<T>) []=(index: i64, value: T): void;
```

### Usage

```whist
var nums = Vec::new();
nums.push(1);
nums.push(2);
nums.push(3);

var doubled = nums.map(|x| x * 2);  // [2, 4, 6]
var sum = nums.fold(0, |acc, x| acc + x);  // 6

foreach n in nums {
    print("{n}\n");
}
```

### Implementation

```whist
struct Vec<T> {
    data: *T,
    len: i64,
    cap: i64,
}

impl<T> Vec<T> {
    func new(): Vec<T> {
        return Vec { data: null, len: 0, cap: 0 };
    }

    func (Vec<T>) push(value: T): void {
        if self.len == self.cap {
            self.grow();
        }
        self.data[self.len] = value;
        self.len += 1;
    }

    func (Vec<T>) grow(): void {
        var new_cap = if self.cap == 0 { 4 } else { self.cap * 2 };
        self.data = realloc(self.data, new_cap * sizeof(T));
        self.cap = new_cap;
    }
}
```

## HashMap<K, V>

Hash table with O(1) average lookup.

### API

```whist
struct HashMap<K, V> {
    // Internal: buckets, count, load factor
}

impl<K: Hash + Eq, V> HashMap<K, V> {
    // Constructors
    func new(): HashMap<K, V>;
    func with_capacity(cap: i64): HashMap<K, V>;

    // Access
    func (HashMap<K, V>) get(key: K): ?V;
    func (HashMap<K, V>) get_or_default(key: K, default: V): V;
    func (HashMap<K, V>) contains_key(key: K): bool;

    // Modification
    func (HashMap<K, V>) insert(key: K, value: V): ?V;  // returns old value
    func (HashMap<K, V>) remove(key: K): ?V;
    func (HashMap<K, V>) clear(): void;

    // Size
    func (HashMap<K, V>) len(): i64;
    func (HashMap<K, V>) is_empty(): bool;

    // Iteration
    func (HashMap<K, V>) keys(): Iterator<K>;
    func (HashMap<K, V>) values(): Iterator<V>;
    func (HashMap<K, V>) iter(): Iterator<(K, V)>;

    // Entry API
    func (HashMap<K, V>) entry(key: K): Entry<K, V>;
}

enum Entry<K, V> {
    Occupied(OccupiedEntry<K, V>),
    Vacant(VacantEntry<K, V>),
}

impl<K, V> Entry<K, V> {
    func or_insert(default: V): *V;
    func or_insert_with(f: func(): V): *V;
    func and_modify(f: func(*V): void): Entry<K, V>;
}
```

### Usage

```whist
var scores = HashMap::new();
scores.insert("Alice", 100);
scores.insert("Bob", 85);

var alice_score = scores.get("Alice") ?? 0;  // 100

// Entry API for get-or-insert
var count = word_counts.entry(word).or_insert(0);
*count += 1;

foreach (name, score) in scores {
    print("{name}: {score}\n");
}
```

### Implementation Notes

- Use Robin Hood hashing or Swiss table
- Default load factor: 0.75
- Grow by 2x when load factor exceeded
- Handle hash collisions with linear probing

## HashSet<T>

Set of unique elements with O(1) lookup.

### API

```whist
struct HashSet<T> {
    // Internally: HashMap<T, ()>
}

impl<T: Hash + Eq> HashSet<T> {
    func new(): HashSet<T>;

    func (HashSet<T>) insert(value: T): bool;  // true if new
    func (HashSet<T>) remove(value: T): bool;
    func (HashSet<T>) contains(value: T): bool;

    func (HashSet<T>) len(): i64;
    func (HashSet<T>) is_empty(): bool;
    func (HashSet<T>) clear(): void;

    // Set operations
    func (HashSet<T>) union(other: HashSet<T>): HashSet<T>;
    func (HashSet<T>) intersection(other: HashSet<T>): HashSet<T>;
    func (HashSet<T>) difference(other: HashSet<T>): HashSet<T>;
    func (HashSet<T>) symmetric_difference(other: HashSet<T>): HashSet<T>;
    func (HashSet<T>) is_subset(other: HashSet<T>): bool;
    func (HashSet<T>) is_superset(other: HashSet<T>): bool;

    func (HashSet<T>) iter(): Iterator<T>;
}
```

### Usage

```whist
var seen = HashSet::new();
seen.insert(1);
seen.insert(2);
seen.insert(1);  // returns false, already present

print(seen.len());  // 2

var a = HashSet::from([1, 2, 3]);
var b = HashSet::from([2, 3, 4]);
var common = a.intersection(b);  // {2, 3}
```

## LinkedList<T>

Doubly-linked list for O(1) insert/remove at ends.

### API

```whist
struct LinkedList<T> { ... }

impl<T> LinkedList<T> {
    func new(): LinkedList<T>;

    func (LinkedList<T>) push_front(value: T): void;
    func (LinkedList<T>) push_back(value: T): void;
    func (LinkedList<T>) pop_front(): ?T;
    func (LinkedList<T>) pop_back(): ?T;

    func (LinkedList<T>) front(): ?T;
    func (LinkedList<T>) back(): ?T;

    func (LinkedList<T>) len(): i64;
    func (LinkedList<T>) is_empty(): bool;
    func (LinkedList<T>) clear(): void;

    func (LinkedList<T>) iter(): Iterator<T>;
}
```

### Usage

```whist
var list = LinkedList::new();
list.push_back(1);
list.push_back(2);
list.push_front(0);

while let Some(x) = list.pop_front() {
    print("{x}\n");  // 0, 1, 2
}
```

## Queue<T> and Stack<T>

Specialized collections for FIFO/LIFO access.

### Queue API

```whist
struct Queue<T> { ... }

impl<T> Queue<T> {
    func new(): Queue<T>;

    func (Queue<T>) enqueue(value: T): void;
    func (Queue<T>) dequeue(): ?T;
    func (Queue<T>) peek(): ?T;

    func (Queue<T>) len(): i64;
    func (Queue<T>) is_empty(): bool;
}
```

### Stack API

```whist
struct Stack<T> { ... }

impl<T> Stack<T> {
    func new(): Stack<T>;

    func (Stack<T>) push(value: T): void;
    func (Stack<T>) pop(): ?T;
    func (Stack<T>) peek(): ?T;

    func (Stack<T>) len(): i64;
    func (Stack<T>) is_empty(): bool;
}
```

### Usage

```whist
// Queue for BFS
var queue = Queue::new();
queue.enqueue(start_node);
while let Some(node) = queue.dequeue() {
    foreach neighbor in node.neighbors {
        queue.enqueue(neighbor);
    }
}

// Stack for DFS or expression parsing
var stack = Stack::new();
stack.push('(');
stack.push('[');
var top = stack.pop();  // Some('[')
```

## BTreeMap<K, V> and BTreeSet<T>

Sorted collections using B-trees.

### API

```whist
impl<K: Ord, V> BTreeMap<K, V> {
    func new(): BTreeMap<K, V>;

    // Same as HashMap, plus:
    func (BTreeMap<K, V>) first_key(): ?K;
    func (BTreeMap<K, V>) last_key(): ?K;
    func (BTreeMap<K, V>) range(from: K, to: K): Iterator<(K, V)>;
}

impl<T: Ord> BTreeSet<T> {
    func new(): BTreeSet<T>;

    // Same as HashSet, plus:
    func (BTreeSet<T>) first(): ?T;
    func (BTreeSet<T>) last(): ?T;
    func (BTreeSet<T>) range(from: T, to: T): Iterator<T>;
}
```

### When to Use

| Use Case | HashMap/Set | BTreeMap/Set |
|----------|-------------|--------------|
| Fast lookup | ✓ O(1) avg | O(log n) |
| Ordered iteration | ✗ | ✓ |
| Range queries | ✗ | ✓ |
| Memory efficiency | Less | More |

## Iterator Trait

Common interface for iteration:

```whist
trait Iterator<T> {
    func next(): ?T;

    // Provided methods
    func count(): i64;
    func last(): ?T;
    func nth(n: i64): ?T;
    func skip(n: i64): Iterator<T>;
    func take(n: i64): Iterator<T>;
    func map<U>(f: func(T): U): Iterator<U>;
    func filter(pred: func(T): bool): Iterator<T>;
    func fold<A>(init: A, f: func(A, T): A): A;
    func collect(): Vec<T>;
    func for_each(f: func(T): void): void;
    func find(pred: func(T): bool): ?T;
    func any(pred: func(T): bool): bool;
    func all(pred: func(T): bool): bool;
    func enumerate(): Iterator<(i64, T)>;
    func zip<U>(other: Iterator<U>): Iterator<(T, U)>;
    func chain(other: Iterator<T>): Iterator<T>;
}
```

### Usage

```whist
var result = numbers
    .iter()
    .filter(|x| x % 2 == 0)
    .map(|x| x * x)
    .take(5)
    .collect();
```

## Implementation Considerations

### Memory Management

With reference counting:
```whist
struct Vec<T> {
    data: owned *T,  // Vec owns the buffer
    len: i64,
    cap: i64,
}

impl<T> Drop for Vec<T> {
    func (Vec<T>) drop(): void {
        // Drop all elements
        foreach i in 0..self.len {
            drop(self.data[i]);
        }
        // Free buffer
        free(self.data);
    }
}
```

### Generic Bounds

Collections require trait bounds:
```whist
// HashMap needs Hash + Eq
impl<K: Hash + Eq, V> HashMap<K, V> { ... }

// BTreeMap needs Ord
impl<K: Ord, V> BTreeMap<K, V> { ... }

// Vec.sort needs Ord
func (Vec<T>) sort(): void where T: Ord { ... }
```

### C Code Generation

```whist
Vec<i64>
```

Generates:
```c
typedef struct {
    int64_t* data;
    int64_t len;
    int64_t cap;
} Vec_i64;

void Vec_i64_push(Vec_i64* self, int64_t value) {
    if (self->len == self->cap) {
        Vec_i64_grow(self);
    }
    self->data[self->len++] = value;
}
```

## Open Questions

1. **Allocation strategy?**
   - Growth factor (1.5x vs 2x)
   - Small buffer optimization

2. **Hash function?**
   - SipHash (secure)
   - FxHash (fast)
   - User-provided

3. **Interior mutability?**
   - RefCell<T> for runtime borrow checking
   - Needed for some patterns

4. **Thread safety?**
   - Single-threaded initially
   - Concurrent collections later

## Related Features

- [Traits](traits.md) - Iterator, Hash, Eq, Ord traits
- [Generics](../PLANS.md) - Generic type parameters
- [Memory Management](memory-management.md) - Collection ownership

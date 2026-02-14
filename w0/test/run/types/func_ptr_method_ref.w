struct Counter { value: i64 }

func (Counter) increment(): void {
    self.value = self.value + 1;
}

func main(): i32 {
    var f: func(Counter): void = Counter.increment;
    return 0;
}

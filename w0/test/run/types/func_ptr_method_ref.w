// Expected: PASS: func_ptr_method_ref

struct Counter { value: i64 }

func (Counter) increment(): void {
    self.value = self.value + 1;
}

test "func_ptr_method_ref" {
    var f: func(Counter): void = Counter.increment;
}

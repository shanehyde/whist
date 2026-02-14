// Expected: PASS: func_ptr_nullable

func noop(): void {
    return;
}

test "func_ptr_nullable" {
    var fp: func(): void = null;
    fp = noop;
}

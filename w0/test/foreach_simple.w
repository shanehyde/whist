extern stdio {
    func printf(s: string): void;
}

func main(): i32 {
    var result = 0;
    var z = 2;

    foreach (const num in 10..14 by z) {
        result = result + num;
    }
    return result;  // Should return 10 + 12 + 14 = 36
}
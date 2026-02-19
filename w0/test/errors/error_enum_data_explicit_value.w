// Expected error: Cannot assign explicit values in enums with data variants

enum Result {
    Ok(i32) = 12,
    Err,
}

func main() -> i32 {
    return 0;
}

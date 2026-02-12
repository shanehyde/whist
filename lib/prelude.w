// Whist prelude — implicitly imported into every compilation unit.
// Provides fundamental types and traits without requiring explicit import.

enum Option<T> {
    Some(T),
    None,
}

enum Result<T, E> {
    Ok(T),
    Err(E),
}

trait Drop {
    func drop(): void;
}

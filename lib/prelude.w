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

func (const Option<T>) has_value(): bool {
    match (self) {
        Some(_) => return true;
        None => return false;
    }
}

func (const Result<T, E>) has_value(): bool {
    match (self) {
        Ok(_) => return true;
        Err(_) => return false;
    }
}

func (const Result<T, E>) is_ok(): bool {
    match (self) {
        Ok(_) => return true;
        Err(_) => return false;
    }
}

func (const Result<T, E>) is_err(): bool {
    match (self) {
        Ok(_) => return false;
        Err(_) => return true;
    }
}

trait Drop {
    func drop(): void;
}

// Whist prelude — implicitly imported into every compilation unit.
// Provides fundamental types and traits without requiring explicit import.

private extern whist_runtime {
    func __whist_panic(msg: string): void as panic;
}

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

func (const Option<T>) value(): T {
    match (self) {
        Some(v) => return v;
        None => {
            panic("Option.value() called on None");
        }
    }
}

func (const Option<T>) expect(msg: string): T {
    match (self) {
        Some(v) => return v;
        None => {
            panic(msg);
        }
    }
}

func (const Option<T>) value_or(def: T): T {
    match (self) {
        Some(v) => return v;
        None => return def;
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

func (const Result<T, E>) value(): T {
    match (self) {
        Ok(v) => return v;
        Err(_) => {
            panic("Result.value() called on Err");
        }
    }
}

func (const Result<T, E>) expect(msg: string): T {
    match (self) {
        Ok(v) => return v;
        Err(_) => {
            panic(msg);
        }
    }
}

func (const Result<T, E>) value_or(def: T): T {
    match (self) {
        Ok(v) => return v;
        Err(_) => return def;
    }
}

func (const Result<T, E>) error(): E {
    match (self) {
        Ok(_) => {
            panic("Result.error() called on Ok");
        }
        Err(e) => return e;
    }
}

func (const Result<T, E>) unwrap_or_else(f: func(E): T): T {
    match (self) {
        Ok(v) => return v;
        Err(e) => return f(e);
    }
}

func (const Result<T, E>) map<U>(f: func(T): U): Result<U, E> {
    match (self) {
        Ok(v) => return Result::Ok(f(v));
        Err(e) => return Result::Err(e);
    }
}

func (const Result<T, E>) map_err<F>(f: func(E): F): Result<T, F> {
    match (self) {
        Ok(v) => return Result::Ok(v);
        Err(e) => return Result::Err(f(e));
    }
}

func (const Result<T, E>) and_then<U>(f: func(T): Result<U, E>): Result<U, E> {
    match (self) {
        Ok(v) => return f(v);
        Err(e) => return Result::Err(e);
    }
}

func (const Option<T>) unwrap_or_else(f: func(): T): T {
    match (self) {
        Some(v) => return v;
        None => return f();
    }
}

func (const Option<T>) map<U>(f: func(T): U): Option<U> {
    match (self) {
        Some(v) => return Option::Some(f(v));
        None => return Option::None;
    }
}

func (const Option<T>) and_then<U>(f: func(T): Option<U>): Option<U> {
    match (self) {
        Some(v) => return f(v);
        None => return Option::None;
    }
}

trait Drop {
    func drop(): void;
}

trait Eq {
    func eq(other: Self): bool;
}

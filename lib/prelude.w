// Whist prelude — implicitly imported into every compilation unit.
// Provides fundamental types and traits without requiring explicit import.

private extern whist_runtime {
    func __whist_panic(msg: string) -> void as panic;
}

public enum Option<T> {
    Some(T),
    None,
}

public enum Result<T, E> {
    Ok(T),
    Err(E),
}

public func (const Option<T>) has_value() -> bool {
    match (self) {
        Some(_) => return true;
        None => return false;
    }
}

public func (const Option<T>) value() -> T {
    match (self) {
        Some(v) => return v;
        None => {
            panic("Option.value() called on None");
        }
    }
}

public func (const Option<T>) expect(msg: string) -> T {
    match (self) {
        Some(v) => return v;
        None => {
            panic(msg);
        }
    }
}

public func (const Option<T>) value_or(def: T) -> T {
    match (self) {
        Some(v) => return v;
        None => return def;
    }
}

public func (const Result<T, E>) has_value() -> bool {
    match (self) {
        Ok(_) => return true;
        Err(_) => return false;
    }
}

public func (const Result<T, E>) is_ok() -> bool {
    match (self) {
        Ok(_) => return true;
        Err(_) => return false;
    }
}

public func (const Result<T, E>) is_err() -> bool {
    match (self) {
        Ok(_) => return false;
        Err(_) => return true;
    }
}

public func (const Result<T, E>) value() -> T {
    match (self) {
        Ok(v) => return v;
        Err(_) => {
            panic("Result.value() called on Err");
        }
    }
}

public func (const Result<T, E>) expect(msg: string) -> T {
    match (self) {
        Ok(v) => return v;
        Err(_) => {
            panic(msg);
        }
    }
}

public func (const Result<T, E>) value_or(def: T) -> T {
    match (self) {
        Ok(v) => return v;
        Err(_) => return def;
    }
}

public func (const Result<T, E>) error() -> E {
    match (self) {
        Ok(_) => {
            panic("Result.error() called on Ok");
        }
        Err(e) => return e;
    }
}

public func (const Result<T, E>) unwrap_or_else(f: func(E) -> T) -> T {
    match (self) {
        Ok(v) => return v;
        Err(e) => return f(e);
    }
}

public func (const Result<T, E>) map<U>(f: func(T) -> U) -> Result<U, E> {
    match (self) {
        Ok(v) => return Result::Ok(f(v));
        Err(e) => return Result::Err(e);
    }
}

public func (const Result<T, E>) map_err<F>(f: func(E) -> F) -> Result<T, F> {
    match (self) {
        Ok(v) => return Result::Ok(v);
        Err(e) => return Result::Err(f(e));
    }
}

public func (const Result<T, E>) and_then<U>(f: func(T) -> Result<U, E>) -> Result<U, E> {
    match (self) {
        Ok(v) => return f(v);
        Err(e) => return Result::Err(e);
    }
}

public func (const Option<T>) unwrap_or_else(f: func() -> T) -> T {
    match (self) {
        Some(v) => return v;
        None => return f();
    }
}

public func (const Option<T>) map<U>(f: func(T) -> U) -> Option<U> {
    match (self) {
        Some(v) => return Option::Some(f(v));
        None => return Option::None;
    }
}

public func (const Option<T>) and_then<U>(f: func(T) -> Option<U>) -> Option<U> {
    match (self) {
        Some(v) => return f(v);
        None => return Option::None;
    }
}

public trait Drop {
    func drop() -> void;
}

public trait Eq {
    func eq(other: Self) -> bool;
}

// --- Vec extension methods ---

public func (const Vec<T>) any(pred: func(T) -> bool) -> bool {
    foreach (const elem in self) {
        if (pred(elem)) {
            return true;
        }
    }
    return false;
}

public func (const Vec<T>) all(pred: func(T) -> bool) -> bool {
    foreach (const elem in self) {
        if (!pred(elem)) {
            return false;
        }
    }
    return true;
}

public func (const Vec<T>) map<K>(transform: func(T) -> K) -> Vec<K> {
    var result = new Vec<K>{};
    foreach (const elem in self) {
        result.push(transform(elem));
    }
    return result;
}

public func (const Vec<T>) filter(pred: func(T) -> bool) -> Vec<T> {
    var result = new Vec<T>{};
    foreach (const elem in self) {
        if (pred(elem)) {
            result.push(elem);
        }
    }
    return result;
}

public func (const Vec<T>) each(f: func(T) -> void) -> void {
    foreach (const elem in self) {
        f(elem);
    }
}

public func (const Vec<T>) is_empty() -> bool {
    return self.count == 0;
}

public func (const Vec<T>) find(pred: func(T) -> bool) -> Option<T> {
    foreach (const elem in self) {
        if (pred(elem)) {
            return Option::Some(elem);
        }
    }
    return Option::None;
}

public func (Vec<T>) extend(other: Vec<T>) -> void {
    foreach (const elem in other) {
        self.push(elem);
    }
}

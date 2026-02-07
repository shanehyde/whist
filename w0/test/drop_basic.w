trait Drop {
    func drop(): void;
}

struct Resource {
    id: i64,
}

impl Drop for Resource {
    func drop(): void {
        // cleanup
    }
}

func main(): i32 {
    var r = new Resource { id: 1 };
    return 0;
}

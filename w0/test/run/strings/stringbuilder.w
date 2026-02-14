// Test StringBuilder: type-check basic operations

func main(): i32 {
    var sb = new StringBuilder{};
    sb.append("hello");
    sb.append_char(' ');
    sb.append_line("world");
    var s: string = sb.to_string();
    var n: i64 = sb.len();
    var c: i64 = sb.capacity();
    sb.clear();
    return 0;
}

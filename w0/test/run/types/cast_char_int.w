// Expected: PASS: cast_char_int
// Test char <-> integer cast expressions

test "cast_char_int" {
    // char -> i32
    var ch: char = 'A';
    var code: i32 = ch as i32;
    assert(code == 65);

    // i32 -> char
    var num: i32 = 66;
    var letter: char = num as char;
    assert(letter == 'B');

    // Literal cast: char literal -> i32
    var val: i32 = 'Z' as i32;
    assert(val == 90);

    // Integer literal -> char
    var c2: char = 67 as char;
    assert(c2 == 'C');

    // Chained cast: char -> i32 -> char
    var original: char = 'D';
    var roundtrip: char = (original as i32) as char;
    assert(roundtrip == 'D');

    // Identity cast: i32 -> i32
    var x: i32 = 42;
    var y: i32 = x as i32;
    assert(y == 42);

    // Integer -> integer widening: i32 -> i64
    var small: i32 = 100;
    var big: i64 = small as i64;
    assert(big == 100);

    // Integer -> integer narrowing: i64 -> i32
    var wide: i64 = 200;
    var narrow: i32 = wide as i32;
    assert(narrow == 200);

    // Cast in expression context
    var digit: char = '5';
    var digit_val: i32 = digit as i32 - 48;
    assert(digit_val == 5);
}

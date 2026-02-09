// Test char <-> integer cast expressions

func main(): i32 {
    // char -> i32
    var ch: char = 'A';
    var code: i32 = ch as i32;
    if (code != 65) {
        return 1;
    }

    // i32 -> char
    var num: i32 = 66;
    var letter: char = num as char;
    if (letter != 'B') {
        return 2;
    }

    // Literal cast: char literal -> i32
    var val: i32 = 'Z' as i32;
    if (val != 90) {
        return 3;
    }

    // Integer literal -> char
    var c2: char = 67 as char;
    if (c2 != 'C') {
        return 4;
    }

    // Chained cast: char -> i32 -> char
    var original: char = 'D';
    var roundtrip: char = (original as i32) as char;
    if (roundtrip != 'D') {
        return 5;
    }

    // Identity cast: i32 -> i32
    var x: i32 = 42;
    var y: i32 = x as i32;
    if (y != 42) {
        return 6;
    }

    // Integer -> integer widening: i32 -> i64
    var small: i32 = 100;
    var big: i64 = small as i64;
    if (big != 100) {
        return 7;
    }

    // Integer -> integer narrowing: i64 -> i32
    var wide: i64 = 200;
    var narrow: i32 = wide as i32;
    if (narrow != 200) {
        return 8;
    }

    // Cast in expression context
    var digit: char = '5';
    var digit_val: i32 = digit as i32 - 48;
    if (digit_val != 5) {
        return 9;
    }

    return 0;
}

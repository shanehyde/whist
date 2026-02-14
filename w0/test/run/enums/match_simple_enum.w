// Expected: PASS: match_simple_enum
// Test match on simple (non-data) enum

enum Direction {
    North,
    South,
    East,
    West,
}

test "match_simple_enum" {
    var d: Direction = Direction::East;

    var result: i64 = 0;
    match (d) {
        North => { result = 1; },
        South => { result = 2; },
        East => { result = 3; },
        West => { result = 4; },
    }

    assert(result == 3);

    // Test qualified variant names
    var d2: Direction = Direction::West;
    match (d2) {
        Direction::North => { result = 1; },
        Direction::South => { result = 2; },
        Direction::East => { result = 3; },
        Direction::West => { result = 4; },
    }

    assert(result == 4);
}

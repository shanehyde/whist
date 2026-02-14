// Test match on simple (non-data) enum

enum Direction {
    North,
    South,
    East,
    West,
}

func main(): i32 {
    var d: Direction = Direction::East;

    var result: i64 = 0;
    match (d) {
        North => { result = 1; },
        South => { result = 2; },
        East => { result = 3; },
        West => { result = 4; },
    }

    if (result != 3) {
        return 1;
    }

    // Test qualified variant names
    var d2: Direction = Direction::West;
    match (d2) {
        Direction::North => { result = 1; },
        Direction::South => { result = 2; },
        Direction::East => { result = 3; },
        Direction::West => { result = 4; },
    }

    if (result != 4) {
        return 2;
    }

    return 0;
}

// Expected error: Match arm type mismatch

enum Flag {
    Yes,
    No,
}

func main() -> i32 {
    var x = match (Flag::Yes) {
        Yes => 1,
        No => "no",
    };
    return 0;
}

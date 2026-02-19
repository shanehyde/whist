import std;
import collections;

struct ReadDirResult {
    files: Vec<string>,
    dirs: Vec<string>
}

func main() -> i32 {

    var args = std::args();

    if(args.any(|a| a == "--help")) {
        std::println("Usage: vec_problem [--help]");
        return 0;
    }

    return 0;
}
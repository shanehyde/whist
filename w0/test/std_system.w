import std;

func main(): i32 {
    if (std.system("echo hello") != 0) {
        return 1;
    }

    return 0;
}

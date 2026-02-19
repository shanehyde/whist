// Test parent directory imports

include "../math.w";

func quadruple(x: i32) -> i32 {
    return twice(twice(x));
}

// Test parent directory imports

import "../math.w";

func quadruple(x: i32) -> i32 {
    return twice(twice(x));
}

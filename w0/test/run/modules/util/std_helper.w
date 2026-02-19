// Helper module that imports std internally

import std;

// Expose a function that uses std internally
func double_abs(x: i64) -> i64 {
    return std::abs_i64(x) * 2;
}

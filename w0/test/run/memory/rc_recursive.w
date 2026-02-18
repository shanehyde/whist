import std;
import time;

// struct Timer {
//     start: i64,
//     desc: string,
// }

// impl Timer {
//     func init(desc: string) {
//         self.start = time.time_ms();
//         self.desc = desc;
//     }
// }

// func (Timer) timelapsed(): i64 {
//     return time.time_ms() - self.start;
// }

// impl Drop for Timer {
//     func drop() {
//         var elapsed = self.timelapsed();
//         std::println($"{self.desc} took {elapsed} ms");
//     }
// }

func dostuff(i: i32) : void {
    // var t = new Timer($"dostuff({i})");
    var v = new Vec<i32>{};
    // if (i <= 0) {
    //     // var t2 = new Timer($"dostuff2({i})");
    //     var x = new Vec<string>{};
    //     x.push("hello");
    //     return;
    // }
    // dostuff(i - 1);
    // return i + dostuff(i - 1);
}

test "rc_recursive" {
    dostuff(10);
    // std::println($"result: {result}");
}
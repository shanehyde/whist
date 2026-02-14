// Expected exit: 15
func main(): i32 {
    var total = 0;
    foreach (const i in 1..6) {
        total = total + i;
    }
    return total;
}

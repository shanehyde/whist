// Expected error: 'Self' can only be used inside trait definitions and impl blocks
func foo(): Self {
    return 0;
}

func main(): i32 {
    return 0;
}

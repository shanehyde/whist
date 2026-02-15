extern std_time {
    func std__clock_gettime_ms(): i64;
}

func time_ms(): i64 {
    return std__clock_gettime_ms();
}
#include <time.h>

static inline long std__clock_gettime_ms()
{
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
    {
        return -1;
    }
    return (long)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
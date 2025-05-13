#include <Timestamp.h>

// Get current timestamp
Timestamp Timestamp::now()
{
    struct timeval tv;
    // Get microseconds and seconds
    // On x86-64 platform, gettimeofday() is no longer a system call, won't enter kernel, multiple calls won't have performance impact
    gettimeofday(&tv, NULL);
    int64_t seconds = tv.tv_sec;
    // Convert to microseconds
    return Timestamp(seconds * kMicroSecondsPerSecond + tv.tv_usec);
}
std::string Timestamp::toString()const
{
    char buf[128] = {0};
    tm *tm_time = localtime(&microSecondsSinceEpoch_);
    snprintf(buf, 128, "%4d/%02d/%02d %02d:%02d:%02d",
             tm_time->tm_year + 1900,
             tm_time->tm_mon + 1,
             tm_time->tm_mday,
             tm_time->tm_hour,
             tm_time->tm_min,
             tm_time->tm_sec);
    return buf;
}
// 2022/08/26 16:29:10
// 20220826 16:29:10.773804
std::string Timestamp::toFormattedString(bool showMicroseconds) const
{
    char buf[64] = {0};
    time_t seconds = static_cast<time_t>(microSecondsSinceEpoch_ / kMicroSecondsPerSecond);
    // Use localtime function to format seconds into calendar time
    tm *tm_time = localtime(&seconds);
    if (showMicroseconds)
    {
        int microseconds = static_cast<int>(microSecondsSinceEpoch_ % kMicroSecondsPerSecond);
        snprintf(buf, sizeof(buf), "%4d/%02d/%02d %02d:%02d:%02d.%06d",
                tm_time->tm_year + 1900,
                tm_time->tm_mon + 1,
                tm_time->tm_mday,
                tm_time->tm_hour,
                tm_time->tm_min,
                tm_time->tm_sec,
                microseconds);
    }
    else
    {
        snprintf(buf, sizeof(buf), "%4d/%02d/%02d %02d:%02d:%02d",
                tm_time->tm_year + 1900,
                tm_time->tm_mon + 1,
                tm_time->tm_mday,
                tm_time->tm_hour,
                tm_time->tm_min,
                tm_time->tm_sec);
    }
    return buf;
}

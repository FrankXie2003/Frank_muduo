#include "Timestamp.h"

#include <sys/time.h>
#include <cstdio>

Timestamp::Timestamp():microSecondsSinceEpoch_(0){}

Timestamp::Timestamp(int64_t microSecondsSinceEpochArg):
    microSecondsSinceEpoch_(microSecondsSinceEpochArg){}

Timestamp Timestamp::now()
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    int64_t seconds = tv.tv_sec;
    return Timestamp(seconds * 1000000 + tv.tv_usec);
}

std::string Timestamp::toString() const
{
    char buf[128] = {0};
    int64_t seconds = microSecondsSinceEpoch_ / 1000000;
    int64_t microseconds = microSecondsSinceEpoch_ % 1000000;
    //localtime不是线程安全的，多线程环境下用localtime_r
    time_t t = static_cast<time_t>(seconds);
    tm tm_time;
    localtime_r(&t, &tm_time);
    snprintf(buf, sizeof(buf), "%4d/%02d/%02d %02d:%02d:%02d.%06ld",
             tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
             tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec,
             static_cast<long>(microseconds));
    return std::string(buf);
}

#ifndef TIME_STAMP_H
#define TIME_STAMP_H

#include <iostream>
#include <string>
#include <sys/time.h>

class Timestamp
{
public:
    Timestamp()
        : microSecondsSinceEpoch_(0)
    {
    }

    explicit Timestamp(int64_t microSecondsSinceEpoch)
        : microSecondsSinceEpoch_(microSecondsSinceEpoch)
    {
    }

    // Get current timestamp
    static Timestamp now();
    std::string toString()const;
    
    // Format: "%4d year %02d month %02d day Week %d %02d:%02d:%02d.%06d", hours:minutes:seconds.microseconds
    std::string toFormattedString(bool showMicroseconds = false) const;

    // Return microseconds of current timestamp
    int64_t microSecondsSinceEpoch() const { return microSecondsSinceEpoch_; }
    // Return seconds of current timestamp
    time_t secondsSinceEpoch() const
    { 
        return static_cast<time_t>(microSecondsSinceEpoch_ / kMicroSecondsPerSecond); 
    }

    // Invalid timestamp, returns a Timestamp with value 0
    static Timestamp invalid()
    {
        return Timestamp();
    }

    // 1 second = 1000*1000 microseconds
    static const int kMicroSecondsPerSecond = 1000 * 1000;

private:
    // Represents microseconds of timestamp (microseconds elapsed since epoch)
    int64_t microSecondsSinceEpoch_;
};

/**
 * Timer needs to compare timestamps, so operators need to be overloaded
 */
inline bool operator<(Timestamp lhs, Timestamp rhs)
{
    return lhs.microSecondsSinceEpoch() < rhs.microSecondsSinceEpoch();
}

inline bool operator==(Timestamp lhs, Timestamp rhs)
{
    return lhs.microSecondsSinceEpoch() == rhs.microSecondsSinceEpoch();
}

// If it's a repeating timer task, this timestamp will be increased
inline Timestamp addTime(Timestamp timestamp, double seconds)
{
    // Convert delay seconds to microseconds
    int64_t delta = static_cast<int64_t>(seconds * Timestamp::kMicroSecondsPerSecond);
    // Return timestamp after adding time
    return Timestamp(timestamp.microSecondsSinceEpoch() + delta);
}

#endif // TIME_STAMP_H
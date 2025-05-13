#ifndef TIMER_H
#define TIMER_H

#include "noncopyable.h"
#include "Timestamp.h"
#include <functional>

/**
 * Timer is used to describe a timer
 * Timer callback function, next timeout moment, time interval for repeating timers, etc.
 */
class Timer : noncopyable
{
public:
    using TimerCallback = std::function<void()>;

    Timer(TimerCallback cb, Timestamp when, double interval)
        : callback_(move(cb)),
          expiration_(when),
          interval_(interval),
          repeat_(interval > 0.0) // Set to 0 for one-time timer
    {
    }

    void run() const 
    { 
        callback_(); 
    }

    Timestamp expiration() const  { return expiration_; }
    bool repeat() const { return repeat_; }

    // Restart timer (if it's a non-repeating event, set expiration time to 0)
    void restart(Timestamp now);

private:
    const TimerCallback callback_;  // Timer callback function
    Timestamp expiration_;          // Next timeout moment
    const double interval_;         // Timeout interval, if it's a one-time timer, this value is 0
    const bool repeat_;             // Whether to repeat (false means it's a one-time timer)
};

#endif // TIMER_H
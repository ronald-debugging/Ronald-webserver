#include <Timer.h>

void Timer::restart(Timestamp now)
{
    if (repeat_)
    {
        // If it's a repeating timer event, continue adding timer events to get the new event expiration time
        expiration_ = addTime(now, interval_);
    }
    else 
    {
        expiration_ = Timestamp();
    }
}
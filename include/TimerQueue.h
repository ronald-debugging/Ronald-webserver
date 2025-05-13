#ifndef TIMER_QUEUE_H
#define TIMER_QUEUE_H

#include "Timestamp.h"
#include "Channel.h"

#include <vector>
#include <set>

class EventLoop;
class Timer;

class TimerQueue
{
public:
    using TimerCallback = std::function<void()>;

    explicit TimerQueue(EventLoop* loop);
    ~TimerQueue();

    // Insert timer (callback function, expiration time, whether to repeat)
    void addTimer(TimerCallback cb,
                  Timestamp when,
                  double interval);
    
private:
    using Entry = std::pair<Timestamp, Timer*>; // Use timestamp as key to get timer
    using TimerList = std::set<Entry>;          // Underlying implementation uses red-black tree, automatically sorted by timestamp

    // Add timer in this loop
    // Thread safe
    void addTimerInLoop(Timer* timer);

    // Function triggered by timer read event
    void handleRead();

    // Reset timerfd_
    void resetTimerfd(int timerfd_, Timestamp expiration);
    
    // Remove all expired timers
    // 1. Get expired timers
    // 2. Reset these timers (destroy or repeat timer tasks)
    std::vector<Entry> getExpired(Timestamp now);
    void reset(const std::vector<Entry>& expired, Timestamp now);

    // Internal method to insert timer
    bool insert(Timer* timer);

    EventLoop* loop_;           // The EventLoop it belongs to
    const int timerfd_;         // timerfd is the timer interface provided by Linux
    Channel timerfdChannel_;    // Encapsulates timerfd_ file descriptor
    // Timer list sorted by expiration
    TimerList timers_;          // Timer queue (internal implementation is red-black tree)

    bool callingExpiredTimers_; // Indicates that expired timers are being retrieved
};

#endif // TIMER_QUEUE_H
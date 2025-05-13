#include <EventLoop.h>
#include <Channel.h>
#include <Logger.h>
#include <Timer.h>
#include <TimerQueue.h>

#include <sys/timerfd.h>
#include <unistd.h>
#include <string.h>

int createTimerfd()
{
    /**
     * CLOCK_MONOTONIC: absolute time
     * TFD_NONBLOCK: non-blocking
     */
    int timerfd = ::timerfd_create(CLOCK_MONOTONIC,
                                    TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerfd < 0)
    {
        LOG_ERROR << "Failed in timerfd_create";
    }
    return timerfd;
}

TimerQueue::TimerQueue(EventLoop* loop)
    : loop_(loop),
      timerfd_(createTimerfd()),
      timerfdChannel_(loop_, timerfd_),
      timers_()
{
    timerfdChannel_.setReadCallback(
        std::bind(&TimerQueue::handleRead, this));
    timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue()
{   
    timerfdChannel_.disableAll();
    timerfdChannel_.remove();
    ::close(timerfd_);
    // Delete all timers
    for (const Entry& timer : timers_)
    {
        delete timer.second;
    }
}

void TimerQueue::addTimer(TimerCallback cb,
                          Timestamp when,
                          double interval)
{
    Timer* timer = new Timer(std::move(cb), when, interval);
    loop_->runInLoop(
        std::bind(&TimerQueue::addTimerInLoop, this, timer));
}

void TimerQueue::addTimerInLoop(Timer* timer)
{
    // Whether the earliest timer trigger time has been replaced
    bool eraliestChanged = insert(timer);

    // We need to reset the timerfd_ trigger time
    if (eraliestChanged)
    {
        resetTimerfd(timerfd_, timer->expiration());
    }
}

// Reset timerfd
void TimerQueue::resetTimerfd(int timerfd_, Timestamp expiration)
{
    struct itimerspec newValue;
    struct itimerspec oldValue;
    memset(&newValue, '\0', sizeof(newValue));
    memset(&oldValue, '\0', sizeof(oldValue));

    // Timeout time - current time
    int64_t microSecondDif = expiration.microSecondsSinceEpoch() - Timestamp::now().microSecondsSinceEpoch();
    if (microSecondDif < 100)
    {
        microSecondDif = 100;
    }

    struct timespec ts;
    ts.tv_sec = static_cast<time_t>(
        microSecondDif / Timestamp::kMicroSecondsPerSecond);
    ts.tv_nsec = static_cast<long>(
        (microSecondDif % Timestamp::kMicroSecondsPerSecond) * 1000);
    newValue.it_value = ts;
    // This function will wake up the event loop
    if (::timerfd_settime(timerfd_, 0, &newValue, &oldValue))
    {
        LOG_ERROR << "timerfd_settime faield()";
    }
}

void ReadTimerFd(int timerfd) 
{
    uint64_t read_byte;
    ssize_t readn = ::read(timerfd, &read_byte, sizeof(read_byte));
    
    if (readn != sizeof(read_byte)) {
        LOG_ERROR << "TimerQueue::ReadTimerFd read_size < 0";
    }
}

// Return deleted timer nodes (std::vector<Entry> expired)
std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now)
{
    std::vector<Entry> expired;
    Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));
    TimerList::iterator end = timers_.lower_bound(sentry);
    std::copy(timers_.begin(), end, back_inserter(expired));
    timers_.erase(timers_.begin(), end);
    
    return expired;
}

void TimerQueue::handleRead()
{
    Timestamp now = Timestamp::now();
    ReadTimerFd(timerfd_);

    std::vector<Entry> expired = getExpired(now);

    // Iterate through expired timers and call their callbacks
    callingExpiredTimers_ = true;
    for (const Entry& it : expired)
    {
        it.second->run();
    }
    callingExpiredTimers_ = false;
    
    // Reset these timers
    reset(expired, now);
}

void TimerQueue::reset(const std::vector<Entry>& expired, Timestamp now)
{
    Timestamp nextExpire;
    for (const Entry& it : expired)
    {
        // If it's a repeating task, continue execution
        if (it.second->repeat())
        {
            auto timer = it.second;
            timer->restart(Timestamp::now());
            insert(timer);
        }
        else
        {
            delete it.second;
        }

        // If timers were reinserted, need to continue resetting timerfd
        if (!timers_.empty())
        {
            resetTimerfd(timerfd_, (timers_.begin()->second)->expiration());
        }
    }
}

bool TimerQueue::insert(Timer* timer)
{
    bool earliestChanged = false;
    Timestamp when = timer->expiration();
    TimerList::iterator it = timers_.begin();
    if (it == timers_.end() || when < it->first)
    {
        // The earliest timer has been replaced
        earliestChanged = true;
    }

    // Insert this new timer into the timer management red-black tree
    timers_.insert(Entry(when, timer));

    return earliestChanged;
}
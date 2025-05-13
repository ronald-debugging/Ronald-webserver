#pragma once

#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>

#include "noncopyable.h"
#include "Timestamp.h"
#include "CurrentThread.h"
#include "TimerQueue.h"
class Channel;
class Poller;

// Event loop class, mainly contains two major modules: Channel and Poller (epoll abstraction)
class EventLoop : noncopyable
{
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    // Start event loop
    void loop();
    // Exit event loop
    void quit();

    Timestamp pollReturnTime() const { return pollRetureTime_; }

    // Execute in the current loop
    void runInLoop(Functor cb);
    // Put the upper-level registered callback function cb into the queue and wake up the thread where the loop is located to execute cb
    void queueInLoop(Functor cb);

    // Wake up the thread where the loop is located through eventfd
    void wakeup();

    // EventLoop's methods => Poller's methods
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
    bool hasChannel(Channel *channel);

    // Determine whether the EventLoop object is in its own thread
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); } // threadId_ is the thread id when EventLoop is created, CurrentThread::tid() is the current thread id
    /**
     * Timer task related functions
     */
    void runAt(Timestamp timestamp, Functor &&cb)
    {
        timerQueue_->addTimer(std::move(cb), timestamp, 0.0);
    }

    void runAfter(double waitTime, Functor &&cb)
    {
        Timestamp time(addTime(Timestamp::now(), waitTime));
        runAt(time, std::move(cb));
    }

    void runEvery(double interval, Functor &&cb)
    {
        Timestamp timestamp(addTime(Timestamp::now(), interval));
        timerQueue_->addTimer(std::move(cb), timestamp, interval);
    }

private:
    void handleRead();        // Event callback bound to the file descriptor wakeupFd_ returned by eventfd. When wakeup() is called, i.e., when an event occurs, handleRead() reads 8 bytes from wakeupFd_ and wakes up the blocked epoll_wait
    void doPendingFunctors(); // Execute upper-level callbacks

    using ChannelList = std::vector<Channel *>;

    std::atomic_bool looping_; // Atomic operation, implemented by CAS at the bottom
    std::atomic_bool quit_;    // Flag to exit loop

    const pid_t threadId_; // Record which thread id created the current EventLoop, i.e., identifies the thread id to which the current EventLoop belongs

    Timestamp pollRetureTime_; // The time when Poller returns the Channels where events occurred
    std::unique_ptr<Poller> poller_;
    std::unique_ptr<TimerQueue> timerQueue_;
    int wakeupFd_; // Function: When mainLoop gets a new user's Channel, it needs to select a subLoop through polling algorithm and wake up subLoop to process the Channel through this member
    std::unique_ptr<Channel> wakeupChannel_;

    ChannelList activeChannels_; // Return the list of all Channels where events are currently detected by Poller

    std::atomic_bool callingPendingFunctors_; // Indicates whether the current loop has callback operations to execute
    std::vector<Functor> pendingFunctors_;    // Store all callback operations that the loop needs to execute
    std::mutex mutex_;                        // Mutex to protect thread-safe operations on the above vector container
};
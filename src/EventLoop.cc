#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>

#include <EventLoop.h>
#include <Logger.h>
#include <Channel.h>
#include <Poller.h>

// Prevent a thread from creating multiple EventLoop instances
thread_local EventLoop *t_loopInThisThread = nullptr;

// Define the default timeout for the Poller IO multiplexing interface
const int kPollTimeMs = 10000; // 10000 milliseconds = 10 seconds
/* After creating a thread, it is uncertain whether the main thread or the child thread will run first.
 * The advantage of using an eventfd to pass data between threads is that multiple threads can synchronize without locking.
 * The minimum kernel version supported by eventfd is Linux 2.6.27. In versions 2.6.26 and earlier, eventfd can also be used, but flags must be set to 0.
 * Function prototype:
 *     #include <sys/eventfd.h>
 *     int eventfd(unsigned int initval, int flags);
 * Parameter description:
 *      initval, initial value of the counter.
 *      flags, EFD_NONBLOCK, set socket to non-blocking.
 *             EFD_CLOEXEC, when fork is executed, the descriptor in the parent process will be automatically closed, and the descriptor in the child process will be retained.
 * Scenario:
 *     eventfd can be used for communication between threads in the same process.
 *     eventfd can also be used for communication between processes with the same ancestry.
 *     If eventfd is used for communication between unrelated processes, the eventfd needs to be placed in shared memory shared by several processes (not tested).
 */
// Create wakeupfd to notify and wake up subReactor to handle new incoming channels
int createEventfd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        LOG_FATAL<<"eventfd error:"<<errno;
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , callingPendingFunctors_(false)
    , threadId_(CurrentThread::tid())
    , poller_(Poller::newDefaultPoller(this))
    , wakeupFd_(createEventfd())
    , wakeupChannel_(new Channel(this, wakeupFd_))
{
    LOG_DEBUG<<"EventLoop created"<<this<<"in thread"<<threadId_;
    if (t_loopInThisThread)
    {
        LOG_FATAL<<"Another EventLoop"<<t_loopInThisThread<<"exists in this thread "<<threadId_;
    }
    else
    {
        t_loopInThisThread = this;
    }
    
    wakeupChannel_->setReadCallback(
        std::bind(&EventLoop::handleRead, this)); // Set the event type for wakeupfd and the callback operation after the event occurs
    
    wakeupChannel_->enableReading(); // Each EventLoop will listen to the EPOLL read event of wakeupChannel_
}
EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll(); // Remove all interested events from the Channel
    wakeupChannel_->remove();     // Remove the Channel from the EventLoop
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

// Start the event loop
void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;

    LOG_INFO<<"EventLoop start looping";

    while (!quit_)
    {
        activeChannels_.clear();
        pollRetureTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        for (Channel *channel : activeChannels_)
        {
            // Poller listens for which channels have events, then reports to EventLoop, notifying the channel to handle the corresponding event
            channel->handleEvent(pollRetureTime_);
        }
        /**
         * Execute the callback operations that need to be processed in the current EventLoop event loop. For the case where the number of threads >= 2, the main work of the IO thread mainloop (mainReactor):
         * accept receives connections => packages the connfd returned by accept as a Channel => TcpServer::newConnection assigns the TcpConnection object to subloop for processing through polling
         *
         * mainloop calls queueInLoop to add the callback to subloop (this callback needs to be executed by subloop, but subloop is still blocked at poller_->poll). queueInLoop wakes up subloop through wakeup
         **/
        doPendingFunctors();
    }
    LOG_INFO<<"EventLoopstop looping";
    looping_ = false;
}

/**
 * Exit the event loop
 * 1. If quit is called successfully in its own thread, it means that the current thread has finished executing the poller_->poll in the loop() function and exited
 * 2. If quit is called to exit the EventLoop not in the thread to which the current EventLoop belongs, it is necessary to wake up the epoll_wait of the thread to which the EventLoop belongs
 *
 * For example, when calling mainloop(IO)'s quit in a subloop(worker), it is necessary to wake up mainloop(IO)'s poller_->poll to let it finish executing the loop() function
 *
 * !!! Note: Normally, mainloop is responsible for handling connection requests and writing callbacks into subloop. Thread-safe queues can be implemented through the producer-consumer model
 * !!!       But muduo uses the wakeup() mechanism, and the wakeupFd_ created by eventfd notify allows communication between mainloop and subloop
 **/
void EventLoop::quit()
{
    quit_ = true;

    if (!isInLoopThread())
    {
        wakeup();
    }
}

// Execute cb in the current loop
void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread()) // Execute the callback in the current EventLoop
    {
        cb();
    }
    else // If cb is executed in a non-current EventLoop thread, it is necessary to wake up the thread where the EventLoop is located to execute cb
    {
        queueInLoop(cb);
    }
}

// Put cb into the queue and wake up the thread where the loop is located to execute cb
void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }

    /**
     * || callingPendingFunctors means that the current loop is executing callbacks, but new callbacks are added to the loop's pendingFunctors_. It is necessary to wake up the corresponding loop thread that needs to execute the above callback operation through a wakeup write event.
     * This ensures that the next poller_->poll() in loop() will not block (blocking would delay the execution of the newly added callback), and then continue to execute the callbacks in pendingFunctors_.
     **/
    if (!isInLoopThread() || callingPendingFunctors_)
    {
        wakeup(); // Wake up the thread where the loop is located
    }
}

void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one))
    {
        LOG_ERROR<<"EventLoop::handleRead() reads"<<n<<"bytes instead of 8";
    }
}

// Used to wake up the thread where the loop is located. Write a data to wakeupFd_, wakeupChannel will have a read event, and the current loop thread will be woken up
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one))
    {
        LOG_ERROR<<"EventLoop::wakeup() writes"<<n<<"bytes instead of 8";
    }
}

// EventLoop methods => Poller methods
void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel)
{
    return poller_->hasChannel(channel);
}

void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_); // Swapping reduces the scope of the lock's critical section, improves efficiency, and avoids deadlock. If functor() is executed in the critical section and functor() calls queueInLoop(), it will cause a deadlock
    }

    for (const Functor &functor : functors)
    {
        functor(); // Execute the callback operation that the current loop needs to execute
    }

    callingPendingFunctors_ = false;
}

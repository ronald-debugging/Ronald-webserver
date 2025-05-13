#include <EventLoopThread.h>
#include <EventLoop.h>

EventLoopThread::EventLoopThread(const ThreadInitCallback &cb,
                                 const std::string &name)
    : loop_(nullptr)
    , exiting_(false)
    , thread_(std::bind(&EventLoopThread::threadFunc, this), name)
    , mutex_()
    , cond_()
    , callback_(cb)
{
}

EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if (loop_ != nullptr)
    {
        loop_->quit();
        thread_.join();
    }
}

EventLoop *EventLoopThread::startLoop()
{
    thread_.start(); // Start the underlying thread by calling start() on the Thread object thread_

    EventLoop *loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this](){return loop_ != nullptr;});
        loop = loop_;
    }
    return loop;
}

// The following method runs in a separate new thread
void EventLoopThread::threadFunc()
{
    EventLoop loop; // Create an independent EventLoop object, which corresponds one-to-one with the above thread (one loop per thread)

    if (callback_)
    {
        callback_(&loop);
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }
    loop.loop();    // Execute EventLoop's loop(), which starts the underlying Poller's poll()
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}
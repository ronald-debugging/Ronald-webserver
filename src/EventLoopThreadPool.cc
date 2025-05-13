#include <memory>

#include <EventLoopThreadPool.h>
#include <EventLoopThread.h>
#include <Logger.h>
EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg)
    : baseLoop_(baseLoop), name_(nameArg), started_(false), numThreads_(0), next_(0), hash_(3)
{
}

EventLoopThreadPool::~EventLoopThreadPool()
{
    // Don't delete loop, it's stack variable
}

void EventLoopThreadPool::start(const ThreadInitCallback &cb)
{
    started_ = true;

    for (int i = 0; i < numThreads_; ++i)
    {
        char buf[name_.size() + 32];
        snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);
        EventLoopThread *t = new EventLoopThread(cb, buf);
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));
        loops_.push_back(t->startLoop()); // Create thread at the bottom, bind a new EventLoop, and return the address of the loop
        hash_.addNode(buf);               // Add the thread to the consistent hash.
    }

    if (numThreads_ == 0 && cb) // Only one thread (baseLoop) runs for the entire server
    {
        cb(baseLoop_);
    }
}

// If working in multithreading, baseLoop_(mainLoop) will assign Channels to subLoops in a polling manner by default
EventLoop *EventLoopThreadPool::getNextLoop(const std::string &key)
{
    size_t index = hash_.getNode(key); // Get index
    if (index >= loops_.size())
    {
        // Handle errors, such as returning baseLoop or throwing an exception
        LOG_ERROR<<"EventLoopThreadPool::getNextLoop ERROR";
        return baseLoop_; // Or return nullptr
    }
    return loops_[index]; // Access loops_ using the index
}


std::vector<EventLoop *> EventLoopThreadPool::getAllLoops()
{
    if (loops_.empty())
    {
        return std::vector<EventLoop *>(1, baseLoop_);
    }
    else
    {
        return loops_;
    }
}
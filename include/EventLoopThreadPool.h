#pragma once

#include <functional>
#include <string>
#include <vector>
#include <memory>

#include "noncopyable.h"
#include "ConsistenHash.h"
class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg);
    ~EventLoopThreadPool();

    void setThreadNum(int numThreads) { numThreads_ = numThreads; }

    void start(const ThreadInitCallback &cb = ThreadInitCallback());

    // If working in multithreading, baseLoop_ (mainLoop) will assign Channel to subLoop in a round-robin way by default
    EventLoop *getNextLoop(const std::string& key);

    std::vector<EventLoop *> getAllLoops(); // Get all EventLoops

    bool started() const { return started_; } // Whether it has started
    const std::string name() const { return name_; } // Get name

private:
    EventLoop *baseLoop_; // The loop created by the user using muduo. If the number of threads is 1, use the user-created loop directly; otherwise, create multiple EventLoops
    std::string name_; // Thread pool name, usually specified by the user. The name of EventLoopThread in the thread pool depends on the thread pool name.
    bool started_; // Whether it has started
    int numThreads_; // Number of threads in the thread pool
    int next_; // The index of the EventLoop selected when a new connection arrives
    std::vector<std::unique_ptr<EventLoopThread>> threads_; // List of IO threads
    std::vector<EventLoop *> loops_; // List of EventLoops in the thread pool, pointing to EventLoop objects created by the EventLoopThread thread function.
    ConsistentHash hash_; // Consistent hash object
};
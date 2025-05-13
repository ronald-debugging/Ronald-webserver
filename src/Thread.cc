#include <Thread.h>
#include <CurrentThread.h>

#include <semaphore.h>

std::atomic_int Thread::numCreated_(0);

Thread::Thread(ThreadFunc func, const std::string &name)
    : started_(false)
    , joined_(false)
    , tid_(0)
    , func_(std::move(func))
    , name_(name)
{
    setDefaultName();
}

Thread::~Thread()
{
    if (started_ && !joined_)
    {
        thread_->detach();                                                  // Thread class provides method to set detached thread, thread will be automatically destroyed after running (non-blocking)
    }
}

void Thread::start()                                                        // A Thread object records the detailed information of a new thread
{
    started_ = true;
    sem_t sem;
    sem_init(&sem, false, 0);                                               // false means not setting inter-process sharing
    // Start thread
    thread_ = std::shared_ptr<std::thread>(new std::thread([&]() {
        tid_ = CurrentThread::tid();                                        // Get thread's tid value
        sem_post(&sem);
        func_();                                                            // Start a new thread specifically to execute this thread function
    }));

    // Must wait here to get the tid value of the newly created thread above
    sem_wait(&sem);
}

// Difference between join() and detach() in C++ std::thread: https://blog.nowcoder.net/n/8fcd9bb6e2e94d9596cf0a45c8e5858a
void Thread::join()
{
    joined_ = true;
    thread_->join();
}

void Thread::setDefaultName()
{
    int num = ++numCreated_;
    if (name_.empty())
    {
        char buf[32] = {0};
        snprintf(buf, sizeof buf, "Thread%d", num);
        name_ = buf;
    }
}

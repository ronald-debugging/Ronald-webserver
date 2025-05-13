#include <stdlib.h>

#include <Poller.h>
#include <EPollPoller.h>

Poller *Poller::newDefaultPoller(EventLoop *loop)
{
    if (::getenv("MUDUO_USE_POLL"))
    {
        return nullptr; // Generate an instance of poll
    }
    else
    {
        return new EPollPoller(loop); // Generate an instance of epoll
    }
}
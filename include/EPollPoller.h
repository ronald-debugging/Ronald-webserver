#pragma once

#include <vector>
#include <sys/epoll.h>

#include "Poller.h"
#include "Timestamp.h"

/**
 * Usage of epoll:
 * 1. epoll_create
 * 2. epoll_ctl (add, mod, del)
 * 3. epoll_wait
 **/

class Channel;

class EPollPoller : public Poller
{
public:
    EPollPoller(EventLoop *loop);
    ~EPollPoller() override;

    // Override the abstract method of base class Poller
    Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;

private:
    static const int kInitEventListSize = 16;

    // Fill active connections
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
    // Update channel, actually calls epoll_ctl
    void update(int operation, Channel *channel);

    using EventList = std::vector<epoll_event>; // In C++, you can omit struct and just write epoll_event

    int epollfd_;      // The fd returned by epoll_create is saved in epollfd_
    EventList events_; // Used to store all file descriptor event sets returned by epoll_wait
};
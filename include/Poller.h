#pragma once

#include <vector>
#include <unordered_map>

#include "noncopyable.h"
#include "Timestamp.h"

class Channel;
class EventLoop;

// Core IO multiplexing module for event dispatcher in muduo library
class Poller
{
public:
    using ChannelList = std::vector<Channel *>;

    Poller(EventLoop *loop);
    virtual ~Poller() = default;

    // Preserve unified interface for all IO multiplexing
    virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;
    virtual void updateChannel(Channel *channel) = 0;
    virtual void removeChannel(Channel *channel) = 0;

    // Check if the parameter channel is in the current Poller
    bool hasChannel(Channel *channel) const;

    // EventLoop can get the specific implementation of default IO multiplexing through this interface
    static Poller *newDefaultPoller(EventLoop *loop);

protected:
    // map's key: sockfd value: channel type that sockfd belongs to
    using ChannelMap = std::unordered_map<int, Channel *>;
    ChannelMap channels_;

private:
    EventLoop *ownerLoop_; // Define the EventLoop that Poller belongs to
};
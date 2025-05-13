#pragma once

#include <functional>
#include <memory>

#include "noncopyable.h"
#include "Timestamp.h"

class EventLoop;

/**
 * Clarify the relationship between EventLoop, Channel, and Poller. In the Reactor model, they correspond to the event dispatcher.
 * Channel can be understood as a conduit that encapsulates sockfd and its interested events such as EPOLLIN, EPOLLOUT, and also binds the specific events returned by poller.
 **/
class Channel : noncopyable
{
public:
    using EventCallback = std::function<void()>; // muduo still uses typedef
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop *loop, int fd);
    ~Channel();

    // After fd is notified by Poller, handle the event. handleEvent is called in EventLoop::loop()
    void handleEvent(Timestamp receiveTime);

    // Set callback function object
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    // Prevent the channel from being manually removed while still executing callback operations
    void tie(const std::shared_ptr<void> &);

    int fd() const { return fd_; }
    int events() const { return events_; }
    void set_revents(int revt) { revents_ = revt; }

    // Set the event status of fd, equivalent to epoll_ctl add/delete
    void enableReading() { events_ |= kReadEvent; update(); }
    void disableReading() { events_ &= ~kReadEvent; update(); }
    void enableWriting() { events_ |= kWriteEvent; update(); }
    void disableWriting() { events_ &= ~kWriteEvent; update(); }
    void disableAll() { events_ = kNoneEvent; update(); }

    // Return the current event status of fd
    bool isNoneEvent() const { return events_ == kNoneEvent; }
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }

    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    // one loop per thread
    EventLoop *ownerLoop() { return loop_; }
    void remove();
private:

    void update();
    void handleEventWithGuard(Timestamp receiveTime);

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop *loop_; // Event loop
    const int fd_;    // fd, the object Poller listens to
    int events_;      // Registered events of interest for fd
    int revents_;     // Specific events returned by Poller
    int index_;

    std::weak_ptr<void> tie_;
    bool tied_;

    // Since the channel can know the specific events that occurred on fd, it is responsible for calling the corresponding event callback operations
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};
#include <sys/epoll.h>

#include <Channel.h>
#include <EventLoop.h>
#include <Logger.h>

const int Channel::kNoneEvent = 0; // No event
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI; // Read event
const int Channel::kWriteEvent = EPOLLOUT; // Write event

// EventLoop: ChannelList Poller
Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop)
    , fd_(fd)
    , events_(0)
    , revents_(0)
    , index_(-1)
    , tied_(false)
{
}

Channel::~Channel()
{
}

// When is the channel's tie method called?  TcpConnection => channel
/*
 * TcpConnection registers the callback functions corresponding to Channel, and the passed-in callback functions are all member methods of TcpConnection.
 * Therefore, it can be said that: Channel's end must be later than TcpConnection object!
 * Here, tie is used to solve the lifetime issue between TcpConnection and Channel, ensuring that the Channel object can be destroyed before TcpConnection is destroyed.
*/
void Channel::tie(const std::shared_ptr<void> &obj)
{
    tie_ = obj;
    tied_ = true;
}
//update and remove => EpollPoller updates the channel's state in poller
/**
 * When the events of the channel represented by fd change, update is responsible for updating the corresponding events of fd in poller
 **/
void Channel::update()
{
    // Through the eventloop to which the channel belongs, call the corresponding method of poller to register the events of fd
    loop_->updateChannel(this);
}

// Remove the current channel from the eventloop to which it belongs
void Channel::remove()
{
    loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime)
{
    if (tied_)
    {
        std::shared_ptr<void> guard = tie_.lock();
        if (guard)
        {
            handleEventWithGuard(receiveTime);
        }
        // If the upgrade fails, do nothing and indicate that the Channel's TcpConnection object no longer exists
    }
    else
    {
        handleEventWithGuard(receiveTime);
    }
}

void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    LOG_INFO<<"channel handleEvent revents:"<<revents_;
    // Close
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) // When TcpConnection corresponding Channel is closed through shutdown, epoll triggers EPOLLHUP
    {
        if (closeCallback_)
        {
            closeCallback_();
        }
    }
    // Error
    if (revents_ & EPOLLERR)
    {
        if (errorCallback_)
        {
            errorCallback_();
        }
    }
    // Read
    if (revents_ & (EPOLLIN | EPOLLPRI))
    {
        if (readCallback_)
        {
            readCallback_(receiveTime);
        }
    }
    // Write
    if (revents_ & EPOLLOUT)
    {
        if (writeCallback_)
        {
            writeCallback_();
        }
    }
}
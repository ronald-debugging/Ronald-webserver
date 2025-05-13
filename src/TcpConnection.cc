#include <functional>
#include <string>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/tcp.h>
#include <sys/sendfile.h>
#include <fcntl.h> // for open
#include <unistd.h> // for close

#include <TcpConnection.h>
#include <Logger.h>
#include <Socket.h>
#include <Channel.h>
#include <EventLoop.h>

static EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL<<" mainLoop is null!";
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop,
                             const std::string &nameArg,
                             int sockfd,
                             const InetAddress &localAddr,
                             const InetAddress &peerAddr)
    : loop_(CheckLoopNotNull(loop))
    , name_(nameArg)
    , state_(kConnecting)
    , reading_(true)
    , socket_(new Socket(sockfd))
    , channel_(new Channel(loop, sockfd))
    , localAddr_(localAddr)
    , peerAddr_(peerAddr)
    , highWaterMark_(64 * 1024 * 1024) // 64M
{
    // Below, set the corresponding callback functions for the channel. The poller notifies the channel that an interested event has occurred, and the channel will call the corresponding callback function.
    channel_->setReadCallback(
        std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(
        std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(
        std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(
        std::bind(&TcpConnection::handleError, this));

    LOG_INFO<<"TcpConnection::ctor:["<<name_.c_str()<<"]at fd="<<sockfd;
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_INFO<<"TcpConnection::dtor["<<name_.c_str()<<"]at fd="<<channel_->fd()<<"state="<<(int)state_;
}

void TcpConnection::send(const std::string &buf)
{
    if (state_ == kConnected)
    {
        if (loop_->isInLoopThread()) // This is the case for a single reactor. When the user calls conn->send, loop_ is the current thread.
        {
            sendInLoop(buf.c_str(), buf.size());
        }
        else
        {
            loop_->runInLoop(
                std::bind(&TcpConnection::sendInLoop, this, buf.c_str(), buf.size()));
        }
    }
}

/**
 * Send data. The application writes quickly, but the kernel sends data slowly. The data to be sent needs to be written into the buffer, and a high water mark callback is set.
 **/
void TcpConnection::sendInLoop(const void *data, size_t len)
{
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;

    if (state_ == kDisconnected) // The connection's shutdown was previously called, so it cannot be sent again.
    {
        LOG_ERROR<<"disconnected, give up writing";
    }

    // Indicates that the channel_ is writing data for the first time or the buffer has no data to send.
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrote = ::write(channel_->fd(), data, len);
        if (nwrote >= 0)
        {
            remaining = len - nwrote;
            if (remaining == 0 && writeCompleteCallback_)
            {
                // Since all data has been sent here, there is no need to set the epollout event for the channel.
                loop_->queueInLoop(
                    std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else // nwrote < 0
        {
            nwrote = 0;
            if (errno != EWOULDBLOCK) // EWOULDBLOCK indicates a normal return when there is no data in non-blocking mode, equivalent to EAGAIN.
            {
                LOG_ERROR<<"TcpConnection::sendInLoop";
                if (errno == EPIPE || errno == ECONNRESET) // SIGPIPE RESET
                {
                    faultError = true;
                }
            }
        }
    }
    /**
     * Indicates that the current write did not send all the data. The remaining data needs to be saved in the buffer.
     * Then register the EPOLLOUT event for the channel. When the poller finds that the TCP send buffer has space, it will notify
     * the corresponding sock->channel and call the channel's registered writeCallback_ callback method.
     * The channel's writeCallback_ is actually the handleWrite callback set by TcpConnection,
     * which sends all the content of the send buffer outputBuffer_.
     **/
    if (!faultError && remaining > 0)
    {
        // The current length of data remaining to be sent in the send buffer.
        size_t oldLen = outputBuffer_.readableBytes();
        if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_)
        {
            loop_->queueInLoop(
                std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
        }
        outputBuffer_.append((char *)data + nwrote, remaining);
        if (!channel_->isWriting())
        {
            channel_->enableWriting(); // Here, the channel's write event must be registered, otherwise the poller will not notify the channel of epollout.
        }
    }
}

void TcpConnection::shutdown()
{
    if (state_ == kConnected)
    {
        setState(kDisconnecting);
        loop_->runInLoop(
            std::bind(&TcpConnection::shutdownInLoop, this));
    }
}

void TcpConnection::shutdownInLoop()
{
    if (!channel_->isWriting()) // Indicates that all data in the current outputBuffer_ has been sent outside
    {
        socket_->shutdownWrite();
    }
}

// Connection established
void TcpConnection::connectEstablished()
{
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading(); // Register the channel's EPOLLIN read event with the poller

    // New connection established, execute callback
    connectionCallback_(shared_from_this());
}
// Connection destroyed
void TcpConnection::connectDestroyed()
{
    if (state_ == kConnected)
    {
        setState(kDisconnected);
        channel_->disableAll(); // Remove all interested events of the channel from the poller
        connectionCallback_(shared_from_this());
    }
    channel_->remove(); // Remove the channel from the poller
}

// Reading is relative to the server. When the client on the other side has data arriving, the server detects EPOLLIN and triggers the callback on this fd. handleRead reads the data sent by the other side.
void TcpConnection::handleRead(Timestamp receiveTime)
{
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if (n > 0) // Data has arrived
    {
        // A readable event has occurred for an established connection user, call the user-provided callback operation onMessage. shared_from_this gets a smart pointer to TcpConnection.
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if (n == 0) // Client disconnected
    {
        handleClose();
    }
    else // An error occurred
    {
        errno = savedErrno;
        LOG_ERROR<<"TcpConnection::handleRead";
        handleError();
    }
}

void TcpConnection::handleWrite()
{
    if (channel_->isWriting())
    {
        int savedErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
        if (n > 0)
        {
            outputBuffer_.retrieve(n);//Retrieve data from the buffer and move the readindex pointer
            if (outputBuffer_.readableBytes() == 0)
            {
                channel_->disableWriting();
                if (writeCompleteCallback_)
                {
                    // TcpConnection object is in its subloop, adding a callback to pendingFunctors_
                    loop_->queueInLoop(
                        std::bind(writeCompleteCallback_, shared_from_this()));
                }
                if (state_ == kDisconnecting)
                {
                    shutdownInLoop(); // Remove TcpConnection from its current loop
                }
            }
        }
        else
        {
            LOG_ERROR<<"TcpConnection::handleWrite";
        }
    }
    else
    {
        LOG_ERROR<<"TcpConnection fd="<<channel_->fd()<<"is down, no more writing";
    }
}

void TcpConnection::handleClose()
{
    LOG_INFO<<"TcpConnection::handleClose fd="<<channel_->fd()<<"state="<<(int)state_;
    setState(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr); // Connection callback
    closeCallback_(connPtr);      // Execute the close connection callback, which is the TcpServer::removeConnection callback method. // must be the last line
}

void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        err = errno;
    }
    else
    {
        err = optval;
    }
    LOG_ERROR<<"TcpConnection::handleError name:"<<name_.c_str()<<"- SO_ERROR:%"<<err;
}

// Execute sendfile in the event loop
void TcpConnection::sendFile(int fileDescriptor, off_t offset, size_t count) {
    if (connected()) {
        if (loop_->isInLoopThread()) { // Determine whether the current thread is the loop thread
            sendFileInLoop(fileDescriptor, offset, count);
        }else{ // If not, wake up the thread running this TcpConnection to execute the Loop loop
            loop_->runInLoop(
                std::bind(&TcpConnection::sendFileInLoop, shared_from_this(), fileDescriptor, offset, count));
        }
    } else {
        LOG_ERROR<<"TcpConnection::sendFile - not connected";
    }
}

// How many bytes were sent
void TcpConnection::sendFileInLoop(int fileDescriptor, off_t offset, size_t count) {
    ssize_t bytesSent = 0;
    size_t remaining = count; // How much data is left to send
    bool faultError = false; // Error flag

    if (state_ == kDisconnecting) { // Indicates that the connection is already disconnected, so no data needs to be sent.
        LOG_ERROR<<"disconnected, give up writing";
        return;
    }

    // Indicates that the Channel is writing data for the first time or the outputBuffer buffer has no data
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0) {
        bytesSent = sendfile(socket_->fd(), fileDescriptor, &offset, remaining);
        if (bytesSent >= 0) {
            remaining -= bytesSent;
            if (remaining == 0 && writeCompleteCallback_) {
                // remaining being 0 means the data is exactly all sent, so there is no need to set the write event listener for it.
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        } else { // If it is a non-blocking no data return error, this is a normal phenomenon equivalent to EAGAIN, otherwise it is an abnormal situation
            if (errno != EWOULDBLOCK) {
                LOG_ERROR<<"TcpConnection::sendFileInLoop";
            }
            if (errno == EPIPE || errno == ECONNRESET) {
                faultError = true;
            }
        }
    }
    // Handle remaining data
    if (!faultError && remaining > 0) {
        // Continue sending remaining data
        loop_->queueInLoop(
            std::bind(&TcpConnection::sendFileInLoop, shared_from_this(), fileDescriptor, offset, remaining));
    }
}
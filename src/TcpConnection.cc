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
    // Set the corresponding callback functions for the channel. When the poller notifies the channel of interested events, the channel will call the corresponding callback functions
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
        if (loop_->isInLoopThread()) // This is for the case of a single reactor, when the user calls conn->send, loop_ is the current thread
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
 * Send data: the application writes fast while the kernel sends data slowly, so we need to write the data to be sent to the buffer and set the water level callback
 **/
void TcpConnection::sendInLoop(const void *data, size_t len)
{
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;

    if (state_ == kDisconnected) // The connection's shutdown has been called before, can't send anymore
    {
        LOG_ERROR<<"disconnected, give up writing";
    }

    // Indicates that channel_ is writing data for the first time or there is no data to send in the buffer
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrote = ::write(channel_->fd(), data, len);
        if (nwrote >= 0)
        {
            remaining = len - nwrote;
            if (remaining == 0 && writeCompleteCallback_)
            {
                // Since all data has been sent here, there's no need to set epollout event for channel
                loop_->queueInLoop(
                    std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else // nwrote < 0
        {
            nwrote = 0;
            if (errno != EWOULDBLOCK) // EWOULDBLOCK indicates normal return when no data in non-blocking mode, equivalent to EAGAIN
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
     * Indicates that the current write did not send all the data, the remaining data needs to be saved in the buffer
     * Then register EPOLLOUT event for channel, when Poller finds space in TCP send buffer, it will notify
     * the corresponding sock->channel, call the writeCallback_ callback method registered for the channel,
     * channel's writeCallback_ is actually the handleWrite callback set by TcpConnection,
     * to send all the content in the outputBuffer_
     **/
    if (!faultError && remaining > 0)
    {
        // The current length of unsent data in the send buffer
        size_t oldLen = outputBuffer_.readableBytes();
        if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_)
        {
            loop_->queueInLoop(
                std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
        }
        outputBuffer_.append((char *)data + nwrote, remaining);
        if (!channel_->isWriting())
        {
            channel_->enableWriting(); // Be sure to register the write event for the channel, otherwise poller will not notify channel of epollout
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
    if (!channel_->isWriting()) // Indicates that all data in outputBuffer_ has been sent out
    {
        socket_->shutdownWrite();
    }
}

// Connection established
void TcpConnection::connectEstablished()
{
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading(); // Register the channel's EPOLLIN read event to poller

    // New connection established, execute callback
    connectionCallback_(shared_from_this());
}
// Connection destroyed
void TcpConnection::connectDestroyed()
{
    if (state_ == kConnected)
    {
        setState(kDisconnected);
        channel_->disableAll(); // Remove all interested events of the channel from poller
        connectionCallback_(shared_from_this());
    }
    channel_->remove(); // Remove the channel from poller
}

// Reading is relative to the server: when the client has data arriving, the server detects EPOLLIN and triggers the callback on this fd; handleRead reads the data sent by the peer
void TcpConnection::handleRead(Timestamp receiveTime)
{
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if (n > 0) // Data received
    {
        // A readable event occurred for a connected user, call the user-provided onMessage callback; shared_from_this gets the smart pointer of TcpConnection
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if (n == 0) // Client disconnected
    {
        handleClose();
    }
    else // Error occurred
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
            outputBuffer_.retrieve(n);// Move the readindex after reading reable area data from the buffer
            if (outputBuffer_.readableBytes() == 0)
            {
                channel_->disableWriting();
                if (writeCompleteCallback_)
                {
                    // In the subloop where the TcpConnection object is located, add the callback to pendingFunctors_
                    loop_->queueInLoop(
                        std::bind(writeCompleteCallback_, shared_from_this()));
                }
                if (state_ == kDisconnecting)
                {
                    shutdownInLoop(); // Remove TcpConnection from the current loop
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
    closeCallback_(connPtr);      // Execute the close connection callback, which is TcpServer::removeConnection callback   // must be the last line
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

// New zero-copy send function
void TcpConnection::sendFile(const char *filename, size_t count)
{
    if (loop_->isInLoopThread()) { // Check if current thread is the loop thread
        sendFileInLoop(filename, count);
    } else { // If not, wake up the thread running this TcpConnection to execute the Loop
        loop_->runInLoop(std::bind(&TcpConnection::sendFileInLoop, this, filename, count));
    }
}

void TcpConnection::sendFileInLoop(const char *filename, size_t count)
{
    // Execute sendfile in event loop
    int fd = open(filename, O_RDONLY);
    if (fd < 0)
    {
        LOG_ERROR << "TcpConnection::sendFileInLoop failed to open file: " << filename;
        return;
    }

    ssize_t bytesSent = 0; // Number of bytes sent
    size_t remaining = count; // Remaining data to send
    bool faultError = false; // Error flag

    if (state_ == kDisconnecting) { // Connection is already disconnected, no need to send data
        close(fd);
        return;
    }

    // First time Channel starts writing data or no data in outputBuffer
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        bytesSent = sendfile(socket_->fd(), fd, nullptr, count);
        if (bytesSent >= 0)
        {
            remaining = count - bytesSent;
            if (remaining == 0 && writeCompleteCallback_)
            {
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else
        {
            bytesSent = 0;
            if (errno != EWOULDBLOCK)
            {
                LOG_ERROR << "TcpConnection::sendFileInLoop";
                if (errno == EPIPE || errno == ECONNRESET)
                {
                    faultError = true;
                }
            }
        }
    }

    if (!faultError && remaining > 0)
    {
        size_t oldLen = outputBuffer_.readableBytes();
        if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_)
        {
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
        }
        outputBuffer_.append(static_cast<char*>(nullptr), remaining);
        if (!channel_->isWriting())
        {
            channel_->enableWriting();
        }
    }
    close(fd);
}
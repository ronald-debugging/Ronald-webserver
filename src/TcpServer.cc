#include <functional>
#include <string.h>

#include <TcpServer.h>
#include <Logger.h>
#include <TcpConnection.h>

static EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL<<"main Loop is NULL!";
    }
    return loop;
}

TcpServer::TcpServer(EventLoop *loop,
                     const InetAddress &listenAddr,
                     const std::string &nameArg,
                     Option option)
    : loop_(CheckLoopNotNull(loop))
    , ipPort_(listenAddr.toIpPort())
    , name_(nameArg)
    , acceptor_(new Acceptor(loop, listenAddr, option == kReusePort))
    , threadPool_(new EventLoopThreadPool(loop, name_))
    , connectionCallback_()
    , messageCallback_()
    , nextConnId_(1)
    , started_(0)
{
    // When a new user connects, the acceptChannel_ bound in the Acceptor class will have a read event, executing handleRead() and calling TcpServer::newConnection callback
    acceptor_->setNewConnectionCallback(
        std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer()
{
    for(auto &item : connections_)
    {
        TcpConnectionPtr conn(item.second);
        item.second.reset();    // Reset the original smart pointer, let the stack TcpConnectionPtr conn point to the object, when conn goes out of scope, the object pointed by the smart pointer can be released
        // Destroy connection
        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed, conn));
    }
}

// Set the number of subloops at the bottom
void TcpServer::setThreadNum(int numThreads)
{
    int numThreads_=numThreads;
    threadPool_->setThreadNum(numThreads_);
}

// Start server listening
void TcpServer::start()
{
    if (started_.fetch_add(1) == 0)    // Prevent a TcpServer object from being started multiple times
    {
        threadPool_->start(threadInitCallback_);    // Start the underlying loop thread pool
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

// When a new user connects, the acceptor will execute this callback operation, responsible for distributing the connection requests received by mainLoop (acceptChannel_ will have read events) to subLoop through polling
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
    // Polling algorithm to select a subLoop to manage the channel corresponding to connfd
    EventLoop *ioLoop = threadPool_->getNextLoop(peerAddr.toIp());
    char buf[64] = {0};
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;  // Not set as atomic because it only executes in mainloop, no thread safety issues
    std::string connName = name_ + buf;

    LOG_INFO<<"TcpServer::newConnection ["<<name_.c_str()<<"]- new connection ["<<connName.c_str()<<"]from %s"<<peerAddr.toIpPort().c_str();
    
    // Get the local IP address and port information bound to sockfd
    sockaddr_in local;
    ::memset(&local, 0, sizeof(local));
    socklen_t addrlen = sizeof(local);
    if(::getsockname(sockfd, (sockaddr *)&local, &addrlen) < 0)
    {
        LOG_ERROR<<"sockets::getLocalAddr";
    }

    InetAddress localAddr(local);
    TcpConnectionPtr conn(new TcpConnection(ioLoop,
                                            connName,
                                            sockfd,
                                            localAddr,
                                            peerAddr));
    connections_[connName] = conn;
    // The callbacks below are set by the user to TcpServer => TcpConnection, while the Channel is bound to the four handlers set by TcpConnection: handleRead, handleWrite... These callbacks are used in the handleXXX functions
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    // Set the callback for how to close the connection
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

    ioLoop->runInLoop(
        std::bind(&TcpConnection::connectEstablished, conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    loop_->runInLoop(
        std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
    LOG_INFO<<"TcpServer::removeConnectionInLoop ["<<
             name_.c_str()<<"] - connection %s"<<conn->name().c_str();

    connections_.erase(conn->name());
    EventLoop *ioLoop = conn->getLoop();
    ioLoop->queueInLoop(
        std::bind(&TcpConnection::connectDestroyed, conn));
}
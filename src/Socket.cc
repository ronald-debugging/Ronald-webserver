#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include <Socket.h>
#include <Logger.h>
#include <InetAddress.h>

Socket::~Socket()
{
    ::close(sockfd_);
}

void Socket::bindAddress(const InetAddress &localaddr)
{
    if (0 != ::bind(sockfd_, (sockaddr *)localaddr.getSockAddr(), sizeof(sockaddr_in)))
    {
        LOG_FATAL<<"bind sockfd:"<<sockfd_ <<"fail";
    }
}

void Socket::listen()
{
    if (0 != ::listen(sockfd_, 1024))
    {
        LOG_FATAL<<"bind sockfd:"<<sockfd_ <<"fail";
    }
}

int Socket::accept(InetAddress *peeraddr)
{
    /**
     * 1. Invalid parameters for the accept function
     * 2. The returned connfd is not set to non-blocking
     * Reactor model: one loop per thread
     * poller + non-blocking IO
     **/
    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    ::memset(&addr, 0, sizeof(addr));
    // fixed : int connfd = ::accept(sockfd_, (sockaddr *)&addr, &len);
    int connfd = ::accept4(sockfd_, (sockaddr *)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (connfd >= 0)
    {
        peeraddr->setSockAddr(addr);
    }
    return connfd;
}

void Socket::shutdownWrite()
{
    if (::shutdown(sockfd_, SHUT_WR) < 0)
    {
        LOG_ERROR<<"shutdownWrite error";
    }
}

void Socket::setTcpNoDelay(bool on)
{
    // TCP_NODELAY is used to disable the Nagle algorithm.
    // The Nagle algorithm is used to reduce the number of small packets transmitted over the network.
    // Setting TCP_NODELAY to 1 disables this algorithm, allowing small packets to be sent immediately.
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
}

void Socket::setReuseAddr(bool on)
{
    // SO_REUSEADDR allows a socket to forcibly bind to a port already used by another socket.
    // This is very useful for server applications that need to restart and bind to the same port.
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}

void Socket::setReusePort(bool on)
{
    // SO_REUSEPORT allows multiple sockets on the same host to bind to the same port number.
    // This is very useful for load balancing incoming connections across multiple threads or processes.
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
}

void Socket::setKeepAlive(bool on)
{
    // SO_KEEPALIVE enables periodic transmission of messages on connected sockets.
    // If the other end does not respond, the connection is considered broken and closed.
    // This is very useful for detecting failed peers in the network.
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
}
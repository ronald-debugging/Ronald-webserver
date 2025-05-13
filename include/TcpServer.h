#pragma once

/**
 * User uses muduo to write server programs
 **/

#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>

#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include "EventLoopThreadPool.h"
#include "Callbacks.h"
#include "TcpConnection.h"
#include "Buffer.h"

// Class used for server programming
class TcpServer
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    enum Option
    {
        kNoReusePort,// Do not allow reuse of local port
        kReusePort,// Allow reuse of local port
    };

    TcpServer(EventLoop *loop,
              const InetAddress &listenAddr,
              const std::string &nameArg,
              Option option = kNoReusePort);
    ~TcpServer();

    void setThreadInitCallback(const ThreadInitCallback &cb) { threadInitCallback_ = cb; }
    void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }

    // Set the number of underlying subloops
    void setThreadNum(int numThreads);
    /**
     * If not listening, start the server (listen).
     * Multiple calls have no side effects.
     * Thread safe.
     */
    void start();

private:
    void newConnection(int sockfd, const InetAddress &peerAddr);
    void removeConnection(const TcpConnectionPtr &conn);
    void removeConnectionInLoop(const TcpConnectionPtr &conn);

    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

    EventLoop *loop_; // baseloop user-defined loop

    const std::string ipPort_;
    const std::string name_;

    std::unique_ptr<Acceptor> acceptor_; // Runs in mainloop, task is to listen for new connection events

    std::shared_ptr<EventLoopThreadPool> threadPool_; // one loop per thread

    ConnectionCallback connectionCallback_;       // Callback when there is a new connection
    MessageCallback messageCallback_;             // Callback when read/write events occur
    WriteCompleteCallback writeCompleteCallback_; // Callback after message is sent

    ThreadInitCallback threadInitCallback_; // Callback for loop thread initialization
    int numThreads_;// Number of threads in the thread pool
    std::atomic_int started_;
    int nextConnId_;
    ConnectionMap connections_; // Store all connections
};
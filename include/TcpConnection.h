#pragma once

#include <memory>
#include <string>
#include <atomic>

#include "noncopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "Timestamp.h"

class Channel;
class EventLoop;
class Socket;

/**
 * TcpServer => Acceptor => A new user connection is obtained through the accept function to get connfd
 * => TcpConnection sets callback => sets to Channel => Poller => Channel callback
 **/

class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop *loop,
                  const std::string &nameArg,
                  int sockfd,
                  const InetAddress &localAddr,
                  const InetAddress &peerAddr);
    ~TcpConnection();

    EventLoop *getLoop() const { return loop_; }
    const std::string &name() const { return name_; }
    const InetAddress &localAddress() const { return localAddr_; }
    const InetAddress &peerAddress() const { return peerAddr_; }

    bool connected() const { return state_ == kConnected; }

    // Send data
    void send(const std::string &buf);
    void sendFile(int fileDescriptor, off_t offset, size_t count); 
    
    // Close half connection
    void shutdown();

    void setConnectionCallback(const ConnectionCallback &cb)
    { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback &cb)
    { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback &cb)
    { writeCompleteCallback_ = cb; }
    void setCloseCallback(const CloseCallback &cb)
    { closeCallback_ = cb; }
    void setHighWaterMarkCallback(const HighWaterMarkCallback &cb, size_t highWaterMark)
    { highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark; }

    // Connection established
    void connectEstablished();
    // Connection destroyed
    void connectDestroyed();

private:
    enum StateE
    {
        kDisconnected, // Already disconnected
        kConnecting,   // Connecting
        kConnected,    // Connected
        kDisconnecting // Disconnecting
    };
    void setState(StateE state) { state_ = state; }

    void handleRead(Timestamp receiveTime);
    void handleWrite();//Handle write event
    void handleClose();
    void handleError();

    void sendInLoop(const void *data, size_t len);
    void shutdownInLoop();
    void sendFileInLoop(int fileDescriptor, off_t offset, size_t count);
    EventLoop *loop_; // Here is baseloop or subloop determined by the number of threads created in TcpServer. If it is multi-Reactor, this loop_ points to subloop. If it is single-Reactor, this loop_ points to baseloop
    const std::string name_;
    std::atomic_int state_;
    bool reading_;//Whether the connection is listening for read events

    // Socket Channel here is similar to Acceptor. Acceptor => mainloop TcpConnection => subloop
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    // These callbacks are also in TcpServer. Users register by writing to TcpServer. TcpServer then passes the registered callbacks to TcpConnection, and TcpConnection registers the callbacks to Channel
    ConnectionCallback connectionCallback_;       // Callback when there is a new connection
    MessageCallback messageCallback_;             // Callback when there is a read/write message
    WriteCompleteCallback writeCompleteCallback_; // Callback after the message is sent
    HighWaterMarkCallback highWaterMarkCallback_; // High water mark callback
    CloseCallback closeCallback_; // Callback for closing connection
    size_t highWaterMark_; // High water mark threshold

    // Data buffer
    Buffer inputBuffer_;    // Buffer for receiving data
    Buffer outputBuffer_;   // Buffer for sending data. User sends to outputBuffer_
};

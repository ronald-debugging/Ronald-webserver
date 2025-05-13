#pragma once

#include <functional>

#include "noncopyable.h"
#include "Socket.h"
#include "Channel.h"

class EventLoop;
class InetAddress;

class Acceptor : noncopyable
{
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress &)>;

    Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
    ~Acceptor();
    // Set the callback function for new connections
    void setNewConnectionCallback(const NewConnectionCallback &cb) { NewConnectionCallback_ = cb; }
    // Check if listening
    bool listenning() const { return listenning_; }
    // Listen on local port
    void listen();

private:
    void handleRead(); // Handle new user connection event

    EventLoop *loop_; // The baseLoop defined by the user, also called mainLoop
    Socket acceptSocket_; // Dedicated socket for receiving new connections
    Channel acceptChannel_; // Dedicated channel for listening to new connections
    NewConnectionCallback NewConnectionCallback_; // Callback function for new connections
    bool listenning_; // Whether it is listening
};
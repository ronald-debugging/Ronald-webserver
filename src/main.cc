#include <string>

#include <TcpServer.h>
#include <Logger.h>
#include <sys/stat.h>
#include <sstream>
#include "AsyncLogging.h"
#include "LFU.h"
#include "memoryPool.h"
// Log file roll size is 1MB (1*1024*1024 bytes)
static const off_t kRollSize = 1*1024*1024;
class EchoServer
{
public:
    EchoServer(EventLoop *loop, const InetAddress &addr, const std::string &name)
        : server_(loop, addr, name)
        , loop_(loop)
    {
        // Register callback functions
        server_.setConnectionCallback(
            std::bind(&EchoServer::onConnection, this, std::placeholders::_1));
        
        server_.setMessageCallback(
            std::bind(&EchoServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        // Set an appropriate number of subloop threads
        server_.setThreadNum(3);
    }
    void start()
    {
        server_.start();
    }

private:
    // Callback function for connection establishment or disconnection
    void onConnection(const TcpConnectionPtr &conn)   
    {
        if (conn->connected())
        {
            LOG_INFO<<"Connection UP :"<<conn->peerAddress().toIpPort().c_str();
        }
        else
        {
            LOG_INFO<<"Connection DOWN :"<<conn->peerAddress().toIpPort().c_str();
        }
    }

    // Read/write event callback
    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time)
    {
        std::string msg = buf->retrieveAllAsString();
        conn->send(msg);
        // conn->shutdown();   // Close the write end, underlying responds with EPOLLHUP => triggers closeCallback_
    }
    TcpServer server_;
    EventLoop *loop_;

};
AsyncLogging* g_asyncLog = NULL;
AsyncLogging * getAsyncLog(){
    return g_asyncLog;
}
 void asyncLog(const char* msg, int len)
{
    AsyncLogging* logging = getAsyncLog();
    if (logging)
    {
        logging->append(msg, len);
    }
}
int main(int argc,char *argv[]) {
    // Step 1: Start logging, double-buffered asynchronous disk write.
    // Create a folder
    const std::string LogDir="logs";
    mkdir(LogDir.c_str(),0755);
    // Use std::stringstream to construct the log file path
    std::ostringstream LogfilePath;
    LogfilePath << LogDir << "/" << ::basename(argv[0]); // Complete log file path
    AsyncLogging log(LogfilePath.str(), kRollSize);
    g_asyncLog = &log;
    Logger::setOutput(asyncLog); // Set output callback for Logger, reconfigure output location
    log.start(); // Start the log backend thread
    // Step 2: Start memory pool and LFU cache
     // Initialize memory pool
    memoryPool::HashBucket::initMemoryPool();

    // Initialize cache
    const int CAPACITY = 5;  
    RonaldCache::RLfuCache<int, std::string> lfu(CAPACITY);
    // Step 3: Start the underlying network module
    EventLoop loop;
    InetAddress addr(8080);
    EchoServer server(&loop, addr, "EchoServer");
    server.start();
 // Main loop starts event loop, epoll_wait blocks and waits for ready events (main loop only registers the listening socket fd, so it only handles new connection events)
    std::cout << "================================================Start Web Server================================================" << std::endl;
    loop.loop();
    std::cout << "================================================Stop Web Server=================================================" << std::endl;
    // Stop logging
    log.stop();
}
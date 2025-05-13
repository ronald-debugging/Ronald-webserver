#include "AsyncLogging.h"
#include <stdio.h>
AsyncLogging::AsyncLogging(const std::string &basename, off_t rollSize, int flushInterval)
    :
      flushInterval_(flushInterval),
      running_(false),
      basename_(basename),
      rollSize_(rollSize),
      thread_(std::bind(&AsyncLogging::threadFunc, this), "Logging"),
      mutex_(),
      cond_(),
      currentBuffer_(new LargeBuffer),
      nextBuffer_(new LargeBuffer),
      buffers_()
{
    currentBuffer_->bzero();
    nextBuffer_->bzero();
    buffers_.reserve(16); // Only maintain queue length of 2~16
}
// This function is called to handle the process where frontend passes LOG_XXX<<"..." to backend, and backend writes log messages to log file
void AsyncLogging::append(const char *logline, int len)
{
    std::lock_guard<std::mutex> lg(mutex_);
    // Buffer has enough space to write
    if (currentBuffer_->avail() > static_cast<size_t>(len))
    {
        currentBuffer_->append(logline, len);
    }
    else
    {
        buffers_.push_back(std::move(currentBuffer_));

        if (nextBuffer_)
        {
            currentBuffer_ = std::move(nextBuffer_);
        }
        else
        {
            currentBuffer_.reset(new LargeBuffer);
        }
        currentBuffer_->append(logline, len);
        // Wake up backend thread to write to disk
        cond_.notify_one();
    }
}

void AsyncLogging::threadFunc()
{
    // output interface for writing to disk
    LogFile output(basename_, rollSize_);
    BufferPtr newbuffer1(new LargeBuffer); // Create new buffer to replace currentbuffer_
    BufferPtr newbuffer2(new LargeBuffer); // Create new buffer2 to replace newBuffer_, to prevent frontend from being unable to write when backend buffers are full
    newbuffer1->bzero();
    newbuffer2->bzero();
    // Buffer array set to 16, used to swap with frontend buffer array
    BufferVector buffersToWrite;
    buffersToWrite.reserve(16);
    while (running_)
    {
        {
            // Mutex protection ensures other frontend threads cannot write to frontend buffer
            std::unique_lock<std::mutex> lg(mutex_);
            if (buffers_.empty())
            {
                cond_.wait_for(lg, std::chrono::seconds(3));
            }
            buffers_.push_back(std::move(currentBuffer_));
            currentBuffer_ = std::move(newbuffer1);
            if (!nextBuffer_)
            {
                nextBuffer_ = std::move(newbuffer2);
            }
            buffersToWrite.swap(buffers_);
        }
        // Take data from write buffer and write to disk through LogFile interface
        for (auto &buffer : buffersToWrite)
        {
            output.append(buffer->data(), buffer->length());
        }

        if (buffersToWrite.size() > 2)
        {
            buffersToWrite.resize(2);
        }

        if (!newbuffer1)
        {
            newbuffer1 = std::move(buffersToWrite.back());
            buffersToWrite.pop_back();
            newbuffer1->reset();
        }
        if (!newbuffer2)
        {
            newbuffer2 = std::move(buffersToWrite.back());
            buffersToWrite.pop_back();
            newbuffer2->reset();
        }
        buffersToWrite.clear(); // Clear backend buffer queue
        output.flush();         // Clear file buffer
    }
    output.flush(); // Ensure everything is flushed
}
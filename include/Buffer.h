#pragma once

#include <vector>
#include <string>
#include <algorithm>
#include <stddef.h>

// Definition of the underlying buffer type for the network library
class Buffer
{
public:
    static const size_t kCheapPrepend = 8; // Initial reserved prependable space size
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initalSize = kInitialSize)
        : buffer_(kCheapPrepend + initalSize)
        , readerIndex_(kCheapPrepend)
        , writerIndex_(kCheapPrepend)
    {
    }

    size_t readableBytes() const { return writerIndex_ - readerIndex_; }
    size_t writableBytes() const { return buffer_.size() - writerIndex_; }
    size_t prependableBytes() const { return readerIndex_; }

    // Return the starting address of readable data in the buffer
    const char *peek() const { return begin() + readerIndex_; }
    void retrieve(size_t len)
    {
        if (len < readableBytes())
        {
            readerIndex_ += len; // Indicates that the application only read part of the readable buffer data, which is of length len. The data from readerIndex_ + len to writerIndex_ is not read yet
        }
        else // len == readableBytes()
        {
            retrieveAll();
        }
    }
    void retrieveAll()
    {
        readerIndex_ = kCheapPrepend;
        writerIndex_ = kCheapPrepend;
    }

    // Convert the Buffer data reported by onMessage function to string type and return
    std::string retrieveAllAsString() { return retrieveAsString(readableBytes()); }
    std::string retrieveAsString(size_t len)
    {
        std::string result(peek(), len);
        retrieve(len); // The above line has already read the readable data from the buffer, so the buffer must be reset here
        return result;
    }

    // buffer_.size - writerIndex_
    void ensureWritableBytes(size_t len)
    {
        if (writableBytes() < len)
        {
            makeSpace(len); // Expand
        }
    }

    // Add data in memory [data, data+len] to the writable buffer
    void append(const char *data, size_t len)
    {
        ensureWritableBytes(len);
        std::copy(data, data+len, beginWrite());
        writerIndex_ += len;
    }
    char *beginWrite() { return begin() + writerIndex_; }
    const char *beginWrite() const { return begin() + writerIndex_; }

    // Read data from fd
    ssize_t readFd(int fd, int *saveErrno);
    // Send data through fd
    ssize_t writeFd(int fd, int *saveErrno);

private:
    // The address of the first element of the underlying array of vector, which is the starting address of the array
    char *begin() { return &*buffer_.begin(); }
    const char *begin() const { return &*buffer_.begin(); }

    void makeSpace(size_t len)
    {
        /**
         * | kCheapPrepend |xxx| reader | writer |                     // xxx indicates the read part in reader
         * | kCheapPrepend | reader ｜          len          |
         **/
        if (writableBytes() + prependableBytes() < len + kCheapPrepend) // That is, len > remaining space before xxx + writer part
        {
            buffer_.resize(writerIndex_ + len);
        }
        else // Here len <= xxx + writer, move reader to start from xxx to make continuous space after xxx
        {
            size_t readable = readableBytes(); // readable = length of reader
            // Copy the data from readerIndex_ to writerIndex_ in the current buffer
            // to the position kCheapPrepend at the beginning of the buffer, so as to make more writable space
            std::copy(begin() + readerIndex_,
                      begin() + writerIndex_,
                      begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }

    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};

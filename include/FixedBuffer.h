#pragma once
#include <string.h>
#include <string>
// Forward declaration of class
class AsyncLogging;
constexpr int kSmallBufferSize = 4000;
constexpr int kLargeBufferSize = 4000 * 1000;

// Fixed buffer class for managing log data storage
// This class provides a fixed-size buffer that allows data to be appended to the buffer and provides related operations
template <int buffer_size>
class FixedBuffer : noncopyable
{
public:
    // Constructor, initialize current pointer to buffer start position
    FixedBuffer()
        : cur_(data_), size_(0)
    {
    }

    // Append data of specified length to buffer
    // If there is enough available space in the buffer, the data is copied to the current pointer position and the current pointer is updated
    void append(const char *buf, size_t len)
    {
        if (avail() > len)
        {
            memcpy(cur_, buf, len); // Copy data to buffer
            add(len);
        }
    }

    // Return the starting address of the buffer
    const char *data() const { return data_; }

    // Return the length of the current valid data in the buffer
    int length() const { return size_; }

    // Return the position of the current pointer
    char *current() { return cur_; }

    // Return the size of the remaining available space in the buffer
    size_t avail() const { return static_cast<size_t>(buffer_size - size_); }

    // Update the current pointer, increase the specified length
    void add(size_t len)
    {
        cur_ += len;
        size_ += len;
    }
    // Reset the current pointer to the start of the buffer
    void reset()
    {
        cur_ = data_;
        size_ = 0;
    }

    // Clear the data in the buffer
    void bzero() { ::bzero(data_, sizeof(data_)); }

    // Convert the data in the buffer to std::string type and return
    std::string toString() const { return std::string(data_, length()); }

private:
    char data_[buffer_size]; // Define fixed-size buffer
    char *cur_;
    int size_;
};

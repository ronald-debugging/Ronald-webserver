#pragma once
#include <string.h>
#include <string>
#include "noncopyable.h"
#include "FixedBuffer.h"
class GeneralTemplate : noncopyable
{
public:
    GeneralTemplate()
        : data_(nullptr),
          len_(0)
    {}

    explicit GeneralTemplate(const char* data, int len)
        : data_(data),
          len_(len)
    {}

    const char* data_;
    int len_;
};
// LogStream class is used to manage log output stream, overloads the output stream operator << to write various types of values into internal buffer
class LogStream : noncopyable
{
public:
    // Define a Buffer type, using fixed-size buffer
    using Buffer = FixedBuffer<kSmallBufferSize>;

    // Append specified length of character data to buffer
    void append(const char *buffer, int len)
    {
        buffer_.append(buffer, len); // Call Buffer's append method
    }

    // Return constant reference to current buffer
    const Buffer &buffer() const
    {
        return buffer_; // Return current buffer
    }

    // Reset buffer, reset current pointer to buffer start
    void reset_buffer()
    {
        buffer_.reset(); // Call Buffer's reset method
    }

    // Overload output stream operator <<, used to write boolean value to buffer
    LogStream &operator<<(bool express);

    // Overload output stream operator <<, used to write short integer to buffer
    LogStream &operator<<(short number);
    // Overload output stream operator <<, used to write unsigned short integer to buffer
    LogStream &operator<<(unsigned short);
    // Overload output stream operator <<, used to write integer to buffer
    LogStream &operator<<(int);
    // Overload output stream operator <<, used to write unsigned integer to buffer
    LogStream &operator<<(unsigned int);
    // Overload output stream operator <<, used to write long integer to buffer
    LogStream &operator<<(long);
    // Overload output stream operator <<, used to write unsigned long integer to buffer
    LogStream &operator<<(unsigned long);
    // Overload output stream operator <<, used to write long long integer to buffer
    LogStream &operator<<(long long);
    // Overload output stream operator <<, used to write unsigned long long integer to buffer
    LogStream &operator<<(unsigned long long);

    // Overload output stream operator <<, used to write float number to buffer
    LogStream &operator<<(float number);
    // Overload output stream operator <<, used to write double precision float number to buffer
    LogStream &operator<<(double);

    // Overload output stream operator <<, used to write character to buffer
    LogStream &operator<<(char str);
    // Overload output stream operator <<, used to write C-style string to buffer
    LogStream &operator<<(const char *);
    // Overload output stream operator <<, used to write unsigned character pointer to buffer
    LogStream &operator<<(const unsigned char *);
    // Overload output stream operator <<, used to write std::string object to buffer
    LogStream &operator<<(const std::string &);
    // (const char*, int) overload
    LogStream& operator<<(const GeneralTemplate& g);
private:
    // Define maximum number size constant
    static constexpr int kMaxNumberSize = 32;

    // For integer types, special processing is needed, template function used to format integer
    template <typename T>
    void formatInteger(T num);

    // Internal buffer object
    Buffer buffer_;
};

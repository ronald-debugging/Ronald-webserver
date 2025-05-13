#include "LogStream.h"
#include <algorithm>

static const char digits[] = "9876543210123456789";

template <typename T>
void LogStream::formatInteger(T num)
{
    if (buffer_.avail() >= kMaxNumberSize)
    {
        char *start = buffer_.current();
        char *cur = start;
        static const char *zero = digits + 9;
        bool negative = (num < 0); // Check if num is negative
        do
        {
            int remainder = static_cast<int>(num % 10);
            (*cur++) = zero[remainder];
            num /= 10;
        } while (num != 0);
        if (negative)
        {
            *cur++ = '-';
        }
        *cur = '\0';
        std::reverse(start, cur);
        int length = static_cast<int>(cur - start);
        buffer_.add(length);
    }
}
// Overload the output stream operator << to write boolean values into the buffer
LogStream &LogStream::operator<<(bool express) {
    buffer_.append(express ? "true" : "false", express ? 4 : 5);
    return *this;
}

// Overload the output stream operator << to write short integers into the buffer
LogStream &LogStream::operator<<(short number) {
    formatInteger(number);
    return *this;
}

// Overload the output stream operator << to write unsigned short integers into the buffer
LogStream &LogStream::operator<<(unsigned short number) {
    formatInteger(number);
    return *this;
}

// Overload the output stream operator << to write integers into the buffer
LogStream &LogStream::operator<<(int number) {
    formatInteger(number);
    return *this;
}

// Overload the output stream operator << to write unsigned integers into the buffer
LogStream &LogStream::operator<<(unsigned int number) {
    formatInteger(number);
    return *this;
}

// Overload the output stream operator << to write long integers into the buffer
LogStream &LogStream::operator<<(long number) {
    formatInteger(number);
    return *this;
}

// Overload the output stream operator << to write unsigned long integers into the buffer
LogStream &LogStream::operator<<(unsigned long number) {
    formatInteger(number);
    return *this;
}

// Overload the output stream operator << to write long long integers into the buffer
LogStream &LogStream::operator<<(long long number) {
    formatInteger(number);
    return *this;
}

// Overload the output stream operator << to write unsigned long long integers into the buffer
LogStream &LogStream::operator<<(unsigned long long number) {
    formatInteger(number);
    return *this;
}

// Overload the output stream operator << to write floating point numbers into the buffer
LogStream &LogStream::operator<<(float number) {
    *this<<static_cast<double>(number);
    return *this;
}

// Overload the output stream operator << to write double precision floating point numbers into the buffer
LogStream &LogStream::operator<<(double number) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.12g", number);
    buffer_.append(buffer, strlen(buffer));
    return *this;
}

// Overload the output stream operator << to write characters into the buffer
LogStream &LogStream::operator<<(char str) {
    buffer_.append(&str, 1);
    return *this;
}

// Overload the output stream operator << to write C-style character strings into the buffer
LogStream &LogStream::operator<<(const char *str) {
    buffer_.append(str, strlen(str));
    return *this;
}

// Overload the output stream operator << to write unsigned character pointers into the buffer
LogStream &LogStream::operator<<(const unsigned char *str) {
    buffer_.append(reinterpret_cast<const char*>(str), strlen(reinterpret_cast<const char*>(str)));
    return *this;
}

// Overload the output stream operator << to write std::string objects into the buffer
LogStream &LogStream::operator<<(const std::string &str) {
    buffer_.append(str.c_str(), str.size());
    return *this;
}

LogStream& LogStream::operator<<(const GeneralTemplate& g)
{
    buffer_.append(g.data_, g.len_);
    return *this;
}
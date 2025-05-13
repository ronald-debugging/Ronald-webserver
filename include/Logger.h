#pragma once

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <errno.h>
#include "LogStream.h"
#include<functional>
#include "Timestamp.h"

#define OPEN_LOGGING

// SourceFile's purpose is to extract the file name
class SourceFile
{
public:
    explicit SourceFile(const char* filename)
        : data_(filename)
    {
        /**
         * Find the last occurrence of / in data to get the specific file name
         * 2022/10/26/test.log
         */
        const char* slash = strrchr(filename, '/');
        if (slash)
        {
            data_ = slash + 1;
        }
        size_ = static_cast<int>(strlen(data_));
    }

    const char* data_;
    int size_;
};
class Logger
{
public:
   enum LogLevel
    {
        TRACE,
        DEBUG,
        INFO,
        WARN,
        ERROR,
        FATAL,
        LEVEL_COUNT,
    };
    Logger(const char *filename, int line, LogLevel level);
    ~Logger();
    // Stream can be changed
    LogStream& stream() { return impl_.stream_; }


    // Output function and flush buffer function
    using OutputFunc = std::function<void(const char* msg, int len)>;
    using FlushFunc = std::function<void()>;
    static void setOutput(OutputFunc);
    static void setFlush(FlushFunc);

private:
    class Impl
    {
    public:
        using LogLevel=Logger::LogLevel;
        Impl(LogLevel level,int savedErrno,const char *filename, int line);
        void formatTime();
        void finish(); // Add a suffix to a log message

        Timestamp time_;
        LogStream stream_;
        LogLevel level_;
        int line_;
        SourceFile basename_;
    };

private:
    Impl impl_;
};

// Get errno information
const char* getErrnoMsg(int savedErrno);
/**
 * Only output when log level is less than the corresponding level
 * For example, if level is set to FATAL, then logLevel is greater than DEBUG and INFO, so DEBUG and INFO level logs won't be output
 */
#ifdef OPEN_LOGGING
#define LOG_DEBUG Logger(__FILE__, __LINE__, Logger::DEBUG).stream()
#define LOG_INFO Logger(__FILE__, __LINE__, Logger::INFO).stream()
#define LOG_WARN Logger(__FILE__, __LINE__, Logger::WARN).stream()
#define LOG_ERROR Logger(__FILE__, __LINE__, Logger::ERROR).stream()
#define LOG_FATAL Logger(__FILE__, __LINE__, Logger::FATAL).stream()
#else
#define LOG(level) LogStream()
#endif
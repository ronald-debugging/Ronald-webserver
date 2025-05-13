#include "Logger.h"
#include "CurrentThread.h"

namespace ThreadInfo
{
    thread_local char t_errnobuf[512]; // Error message buffer independent for each thread
    thread_local char t_timer[64];     // Time formatting buffer independent for each thread
    thread_local time_t t_lastSecond;  // Each thread records the last formatted time

}
const char *getErrnoMsg(int savedErrno)
{
    return strerror_r(savedErrno, ThreadInfo::t_errnobuf, sizeof(ThreadInfo::t_errnobuf));
}
// Return the level name based on Level
const char *getLevelName[Logger::LogLevel::LEVEL_COUNT]{
    "TRACE ",
    "DEBUG ",
    "INFO  ",
    "WARN  ",
    "ERROR ",
    "FATAL ",
};
/**
 * Default log output function
 * Writes log content to standard output stream (stdout)
 * @param data Log data to output
 * @param len Length of log data
 */
static void defaultOutput(const char *data, int len)
{
    fwrite(data, len, sizeof(char), stdout);
}

/**
 * Default flush function
 * Flushes the standard output stream buffer to ensure logs are output promptly
 * Called when an error occurs or logs need to be seen immediately
 */
static void defaultFlush()
{
    fflush(stdout);
}
Logger::OutputFunc g_output = defaultOutput;
Logger::FlushFunc g_flush = defaultFlush;

Logger::Impl::Impl(Logger::LogLevel level, int savedErrno, const char *filename, int line)
    : time_(Timestamp::now()),
      stream_(),
      level_(level),
      line_(line),
      basename_(filename)
{
    // Format the current time string according to the timezone, also the beginning of a log message
    formatTime();
    // Write log level
    stream_ << GeneralTemplate(getLevelName[level], 6);
    if (savedErrno != 0)
    {
        stream_ << getErrnoMsg(savedErrno) << " (errno=" << savedErrno << ") ";
    }
}
// Format the current time string according to the timezone, also the beginning of a log message
void Logger::Impl::formatTime()
{
    Timestamp now = Timestamp::now();
    // Calculate seconds
    time_t seconds = static_cast<time_t>(now.microSecondsSinceEpoch() / Timestamp::kMicroSecondsPerSecond);
    int microseconds = static_cast<int>(now.microSecondsSinceEpoch() % Timestamp::kMicroSecondsPerSecond);
    // Calculate remaining microseconds
    struct tm *tm_timer = localtime(&seconds);
    // Write to the time buffer stored by this thread
    snprintf(ThreadInfo::t_timer, sizeof(ThreadInfo::t_timer), "%4d/%02d/%02d %02d:%02d:%02d",
             tm_timer->tm_year + 1900,
             tm_timer->tm_mon + 1,
             tm_timer->tm_mday,
             tm_timer->tm_hour,
             tm_timer->tm_min,
             tm_timer->tm_sec);
    // Update the last time call
    ThreadInfo::t_lastSecond = seconds;

    // muduo uses Fmt to format integers, here we write directly to buf
    char buf[32] = {0};
    snprintf(buf, sizeof(buf), "%06d ", microseconds);

    
    stream_ << GeneralTemplate(ThreadInfo::t_timer, 17) << GeneralTemplate(buf, 7);
}
void Logger::Impl::finish()
{
    stream_ << " - " << GeneralTemplate(basename_.data_, basename_.size_)
            << ':' << line_ << '\n';
}
Logger::Logger(const char *filename, int line, LogLevel level) : impl_(level, 0, filename, line)
{
}
Logger::~Logger()
{
    impl_.finish();
    const LogStream::Buffer &buffer = stream().buffer();
    // Output (default terminal output)
    g_output(buffer.data(), buffer.length());
    // Terminate program in FATAL case
    if (impl_.level_ == FATAL)
    {
        g_flush();
        abort();
    }
}

void Logger::setOutput(OutputFunc out)
{
    g_output = out;
}

void Logger::setFlush(FlushFunc flush)
{
    g_flush = flush;
}
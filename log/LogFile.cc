#include "LogFile.h"
LogFile::LogFile(const std::string &basename,
                 off_t rollsize,
                 int flushInterval,
                 int checkEveryN ) : basename_(basename),
                                           rollsize_(rollsize),
                                           flushInterval_(flushInterval),
                                           checkEveryN_(checkEveryN),
                                           startOfPeriod_(0),
                                           lastRoll_(0),
                                           lastFlush_(0)
{
    // When restarting, there might not be a log file, so when constructing the logFile object, directly call rollfile() to create a new log file
    rollFile();
}
LogFile::~LogFile() = default;
void LogFile::append(const char *data, int len)
{
    std::lock_guard<std::mutex> lg(mutex_);
    appendInlock(data, len);
}
void LogFile::flush()
{
    file_->flush();
}
// Roll log file
bool LogFile::rollFile()
{
    time_t now = 0;
    std::string filename = getLogFileName(basename_, &now);
    time_t start = now / kRollPerSeconds_ * kRollPerSeconds_;
    if (now > lastRoll_)
    {
        lastFlush_ = now;
        lastRoll_ = now;
        startOfPeriod_ = start;
        // Make file_ point to a file named filename, equivalent to creating a new file, but each rollfile() call creates a new file object to write data to the log file
        file_.reset(new FileUtil(filename));
        return true;
    }
    return false;
}
// Log format: basename+now+".log"
std::string LogFile::getLogFileName(const std::string &basename, time_t *now)
{
    std::string filename;
    filename.reserve(basename.size() + 64);
    filename = basename;

    char timebuf[32];
    struct tm tm;
    *now = time(NULL); // Get current time
    localtime_r(now, &tm);
    strftime(timebuf, sizeof(timebuf), ".%Y%m%d-%H%M%S", &tm);

    filename += timebuf;
    filename += ".log";
    return filename;
}
void LogFile::appendInlock(const char *data, int len)
{
    file_->append(data, len);

    time_t now = time(NULL); // Current time
    ++count_;

    // 1. Check if log rolling is needed
    if (file_->writtenBytes() > rollsize_)
    {
        rollFile();
    }
    else if (count_ >= checkEveryN_) // After reaching write count threshold, perform check
    {
        count_ = 0;

        // Roll log based on time period
        time_t thisPeriod = now / kRollPerSeconds_ * kRollPerSeconds_;
        if (thisPeriod != startOfPeriod_)
        {
            rollFile();
        }
    }

    // 2. Check if log needs to be flushed (independent flush logic)
    if (now - lastFlush_ > flushInterval_)
    {
        lastFlush_ = now;
        file_->flush();
    }
}


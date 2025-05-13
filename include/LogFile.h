#pragma once
#include "FileUtil.h"
#include <mutex>
#include <memory>
#include <ctime>
/**
 * @brief Log file management class
 * Responsible for log file creation, writing, rolling and flushing operations
 * Supports automatic log file rolling by size and time
 */
class LogFile
{
public:
    /**
     * @brief Constructor
     * @param basename Basic name of log file
     * @param rollsize When log file size reaches this many bytes, roll the file, unit: bytes
     * @param flushInterval Log flush interval time, default 3 seconds
     * @param checkEveryN_ Check if rolling is needed after writing checkEveryN_ times, default 1024 times
     */
    LogFile(const std::string &basename,
            off_t rollsize,
            int flushInterval = 3,
            int checkEveryN_ = 1024);
    ~LogFile();
    /**
     * @brief Append data to log file
     * @param data Data to be written
     * @param len Data length
     */
    void append(const char *data,int len);

    /**
     * @brief Force flush buffer data to disk
     */
    void flush();

    /**
     * @brief Roll log file
     * Create new log file when log file size exceeds rollsize_ or time exceeds one day
     * @return Whether roll log file successfully
     */
    bool rollFile();

private:
    /**
     * @brief Disable destructor, use smart pointer management
     */


    /**
     * @brief Generate log file name
     * @param basename Basic name of log file
     * @param now Current time pointer
     * @return Complete log file name, format: basename.YYYYmmdd-HHMMSS.log
     */
    static std::string getLogFileName(const std::string &basename, time_t *now);

    /**
     * @brief Append data in locked state
     * @param data Data to be written
     * @param len Data length
     */
    void appendInlock(const char *data, int len);

    const std::string basename_;
    const off_t rollsize_;    // Roll file size
    const int flushInterval_; // Flush time limit, default 3s
    const int checkEveryN_;   // Write data count limit, default 1024

    int count_; // Write data count, cleared when exceeding limit checkEveryN_, then recount

    std::mutex mutex_;
    time_t startOfPeriod_;// Start time of current log writing period (seconds)
    time_t lastRoll_;// Last log file roll time (seconds)
    time_t lastFlush_; // Last log file flush time (seconds)
    std::unique_ptr<FileUtil> file_;
    const static int kRollPerSeconds_ = 60*60*24;
};

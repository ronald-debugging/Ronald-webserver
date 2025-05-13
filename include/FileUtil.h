#pragma once
#include<string>
#include<stdio.h>
#include<sys/types.h>//off_t
/**
 * @brief File utility class for handling file write operations
 * This class encapsulates basic file operations, including writing data and flushing the buffer
 */
class FileUtil
{
public:
    /**
     * @brief Constructor
     * @param file_name Name of the file to open
     */
    FileUtil(std::string& file_name);

    /**
     * @brief Destructor
     * Responsible for closing the file and cleaning up resources
     */
    ~FileUtil();

    /**
     * @brief Write data to file
     * @param data Pointer to the data to write
     * @param len Length of the data to write
     */
    void append(const char* data, size_t len);

    /**
     * @brief Flush file buffer
     * Immediately write the data in the buffer to the file
     */
    void flush();

    /**
     * @brief Get the number of bytes written
     * @return Returns the total number of bytes written to the file
     */
    off_t writtenBytes() const { return writtenBytes_; }  

private:
    size_t write(const char* data, size_t len);
    FILE* file_;                  // File pointer for file operations
    char buffer_[64*1024];       // Buffer for file operations, 64KB in size, used to improve write efficiency
    off_t writtenBytes_;        // Records the total number of bytes written to the file, off_t type for large file support
};
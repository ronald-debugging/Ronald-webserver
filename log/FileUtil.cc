#include <cstring>
#include "FileUtil.h"

FileUtil::FileUtil(std::string &file_name) : file_(::fopen(file_name.c_str(), "ae")),
                                             writtenBytes_(0)
{
    // Set file_ buffer to local buffer to reduce IO operations
    ::setbuffer(file_, buffer_, sizeof(buffer_));
}
FileUtil::~FileUtil()
{
    if (file_)
    {
        ::fclose(file_);
    }
}
// Write data to file
void FileUtil::append(const char *data, size_t len)
{
    size_t writen = 0;
    while (writen != len)
    {
        size_t remain = len - writen;
        size_t n = write(data + writen, remain);
        if (n != remain)
        {
            // Error checking
            int err = ferror(file_);
            if (err)
            {
                fprintf(stderr, "AppendFile::append() failed %s\n", strerror(err));
                clearerr(file_); // Clear file pointer error flag
                break;
            }
        }
        writen += n;
    }
    writtenBytes_ += writen;
}

void FileUtil::flush()
{
    ::fflush(file_);
}
// Actually write data to file
size_t FileUtil::write(const char *data, size_t len)
{
    // Using non-thread-safe fwrite() for performance reasons
   return  ::fwrite_unlocked(data, 1, len, file_);
}
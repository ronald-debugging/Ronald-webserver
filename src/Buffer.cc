#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

#include <Buffer.h>

/**
 * Read data from fd, Poller works in LT mode
 * Buffer has a size limit! But when reading data from fd, we don't know the final size of the TCP data
 * 
 * @description: The method to read from socket to buffer uses readv to read into buffer_ first,
 * If buffer_ space is not enough, it will read into a 65536-byte space on the stack, then append
 * to buffer_. This approach avoids the overhead of system calls while not affecting data reception.
 **/
ssize_t Buffer::readFd(int fd, int *saveErrno)
{
    // Extra stack space, used when reading from socket and buffer_ is temporarily insufficient,
    // temporarily store data until buffer_ is reallocated with enough space, then swap data to buffer_.
    char extrabuf[65536] = {0}; // Stack memory space 65536/1024 = 64KB

    /*
    struct iovec {
        ptr_t iov_base; // The buffer pointed to by iov_base stores the data received by readv or the data to be sent by writev
        size_t iov_len; // In different situations, iov_len determines the maximum length to receive or the actual length to write
    };
    */

    // Use iovec to allocate two consecutive buffers
    struct iovec vec[2];
    const size_t writable = writableBytes(); // This is the remaining writable space in the buffer

    // The first buffer, points to writable space
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;
    // The second buffer, points to stack space
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    // when there is enough space in this buffer, don't read into extrabuf.
    // when extrabuf is used, we read 128k-1 bytes at most.
    // The reason for saying at most 128k-1 bytes is: if writable is 64k-1, then two buffers are needed, the first is 64k-1, the second is 64k, so at most 128k-1
    // If the first buffer >= 64k, then only one buffer is used and the stack space extrabuf[65536] is not used
    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);

    if (n < 0)
    {
        *saveErrno = errno;
    }
    else if (n <= writable) // Buffer is enough
    {
        writerIndex_ += n;
    }
    else // extrabuf is also written
    {
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable); // writerIndex_ begins to write from buffer_.size()
    }
    return n;
}

// inputBuffer_.readFd reads data from the peer into inputBuffer_, moving the writerIndex_ pointer
// outputBuffer_.writeFd writes data to outputBuffer_, starting from readerIndex_, can write readableBytes() bytes
/**
 * @description: The method to write data from buffer to fd uses writev
 * @param {int} fd - Socket file descriptor
 * @param {int} *saveErrno - Error number
 * @return {ssize_t} - Number of bytes written
 */
ssize_t Buffer::writeFd(int fd, int *saveErrno)
{
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0)
    {
        *saveErrno = errno;
    }
    return n;
}
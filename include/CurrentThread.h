#pragma once

#include <unistd.h>
#include <sys/syscall.h>
namespace CurrentThread
{
    extern thread_local int t_cachedTid; // Cache tid because system calls are very time-consuming, save tid after getting it

    void cacheTid();

    inline int tid() // Inline function only works in current file
    {
        if (__builtin_expect(t_cachedTid == 0, 0)) // __builtin_expect is a low-level optimization, this statement means if tid hasn't been obtained yet, enter if and get tid through cacheTid() system call
        {
            cacheTid();
        }
        return t_cachedTid;
    }
}
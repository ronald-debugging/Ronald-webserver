#include "CurrentThread.h"

namespace CurrentThread
{
      thread_local int t_cachedTid = 0; // Define thread local variable in source file
    void cacheTid()
    {
        if (t_cachedTid == 0)
        {
            t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid)); // Ensure syscall and SYS_gettid are defined
        }
    }
}
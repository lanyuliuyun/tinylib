
#include "tinylib/util/util.h"
#include "tinylib/util/log.h"

#ifdef WIN32

#include <windows.h>

#ifdef _MSC_VER
__declspec(thread) int t_cachedTid = 0;
#elif defined(__GNUC__)
__thread int t_cachedTid = 0;
#endif

int current_tid(void)
{
    if (t_cachedTid == 0)
    {
        t_cachedTid = (int)GetCurrentThreadId();
    }

    return t_cachedTid;
}

unsigned long long now_ms(void)
{
    FILETIME now_fs;
    GetSystemTimeAsFileTime(&now_fs);

    return ((ULARGE_INTEGER*)&now_fs)->QuadPart / 10000;
}

unsigned long long ts_ms(void)
{
    return now_ms();
}

#elif defined(__linux__)

#include <time.h>
#include <sys/time.h>
#include <sys/syscall.h>    /* for SYS_gettid */
#include <unistd.h>         /* for syscall() */

__thread int t_cachedTid = 0;

int current_tid(void)
{
    if (t_cachedTid == 0)
    {
        t_cachedTid = (int)syscall(SYS_gettid);
    }

    return t_cachedTid;
}

unsigned long long now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

unsigned long long ts_ms(void)
{
    struct timespec tspec;
    clock_gettime(CLOCK_MONOTONIC, &tspec);

    return tspec.tv_sec * 1000 + tspec.tv_nsec / 1000000;
}

#endif

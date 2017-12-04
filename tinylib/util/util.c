
#include "tinylib/util/util.h"
#include "tinylib/util/log.h"

#ifdef WIN32

#include <windows.h>

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

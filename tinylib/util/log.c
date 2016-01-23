
#include "log.h"

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>

#ifdef WIN32
#include <windows.h>		/* for GetLocalTime() */
#else
#include <sys/time.h>		/* for gettimeofday() */
#include <sys/syscall.h>	/* for SYS_gettid */
#include <unistd.h>			/* for syscall() */
#endif

static log_level_e g_log_level = LOG_LEVEL_INFO;
static FILE *g_out_fp = NULL;

static
void log_write(log_level_e level, const char *file, int line, const char *fmt, va_list ap)
{
	char content[1024];
    char time_head[28]; /* 2014-08-17 12:37:00.555 */
    const char *level_text = "NONE";
	
	FILE *fp;

  #ifdef WIN32
	SYSTEMTIME systime;
	DWORD threadId;
  #else
	struct timeval tv;
    time_t tt;
    struct tm *tm;
	pid_t threadId;
  #endif

    if (g_log_level < level)
    {
        return;
    }

    if (LOG_LEVEL_LOG == level)
    {
    	level_text = "LOG";
    }
    else if (LOG_LEVEL_ERROR == level)
    {
    	level_text = "ERROR";
    }
    else if (LOG_LEVEL_WARN == level)
    {
    	level_text = "WARN";
    }
    else if (LOG_LEVEL_INFO == level)
    {
    	level_text = "INFO";
    }
    else if (LOG_LEVEL_DEBUG == level)
    {
    	level_text = "DEBUG";
    }

	memset(time_head, 0, sizeof(time_head));
  #ifdef WIN32
	GetLocalTime(&systime);
	snprintf(time_head, sizeof(time_head)-1, "%u-%02u-%02u %02u:%02u:%02u.%03u", 
		systime.wYear, systime.wMonth, systime.wDay, systime.wHour, systime.wMinute, systime.wSecond, systime.wMilliseconds);
	threadId = GetCurrentThreadId();
  #else
	gettimeofday(&tv, NULL);
    tt = tv.tv_sec;
    tm = localtime(&tt);
    snprintf(time_head, sizeof(time_head)-1, "%u-%02u-%02u %02u:%02u:%02u.%03u", 
		tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, (unsigned)(tv.tv_usec/1000));
	threadId = syscall(SYS_gettid);
  #endif

    memset(content, 0, sizeof(content));
    vsnprintf(content, sizeof(content)-1, fmt, ap);

	fp = g_out_fp;
	if (NULL == fp)
	{
		fp = stderr;
	}

	fprintf(fp, "[%s][%s] thread: %u on %s:%d %s\n", time_head, level_text, ((unsigned)threadId), file, line, content);

    return;
}

static print_f g_print = log_write;

void log_init(print_f printcb)
{
    if (NULL != printcb)
    {
        g_print = printcb;
    }
}

void log_file(const char *file)
{
	FILE *fp = fopen(file, "w");
	if (NULL != fp)
	{
		setvbuf(fp, NULL, _IONBF, 0);
		g_out_fp = fp;
	}

	return;
}

void log_setlevel(log_level_e level)
{
    g_log_level = level;
}

void log_print(log_level_e level, const char *file, int line, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    g_print(level, file, line, fmt, ap);
    va_end(ap);

    return;
}


#include "util.h"
#include "log.h"

#ifdef _MSC_VER

#include <windows.h>

void get_current_timestamp(unsigned long long *timestamp)
{
	FILETIME now;
	ULARGE_INTEGER ularg;

	if (NULL == timestamp)
	{
		log_error("get_current_timestamp: bad timestamp");
		return;
	}

	GetSystemTimeAsFileTime(&now);
	memcpy(&ularg, &now, sizeof(ularg));

	/* ularg单位为100ns，除以10000转换成ms */
	*timestamp = ularg.QuadPart / 10000;

	return;
}

#else

#include <sys/time.h>

void get_current_timestamp(unsigned long long *timestamp)
{
	struct timeval tv;

	if (NULL == timestamp)
	{
		log_error("get_current_timestamp: bad timestamp");
		return;
	}

	gettimeofday(&tv, NULL);
	*timestamp = tv.tv_sec * 1000 + tv.tv_usec / 1000;

	return;
}

#endif

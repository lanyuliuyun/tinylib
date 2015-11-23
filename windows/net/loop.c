
#include "net/loop.h"
#include "net/timer_queue.h"
#include "async_task_queue.h"
#include "util/log.h"
#include "util/util.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <winsock2.h>

struct loop
{
    int run;
	DWORD threadId;

	struct pollfd *pollfds;
    channel_t **channels;
	channel_t **active_channels;
    unsigned max_count;
    unsigned count;

	async_task_queue_t *task_queue;
    timer_queue_t *timer_queue;
};

loop_t* loop_new(unsigned hint)
{
    loop_t* loop;

    loop = (loop_t*)malloc(sizeof(loop_t));
    memset(loop, 0, sizeof(*loop));
	loop->threadId = 0;
    loop->run = 0;
	
	if (hint < 16)
	{
		hint = 16;
	}

    loop->max_count = hint;
    loop->count = 0;

    loop->pollfds = (struct pollfd*)malloc(loop->max_count * sizeof(struct pollfd));
	memset(loop->pollfds, 0, loop->max_count * sizeof(struct pollfd));

	/* 此处×2，额外的channel是用于保存活跃的channel */
    loop->channels = (channel_t**)malloc(loop->max_count * sizeof(channel_t*) * 2);
    if (NULL == loop->channels)
    {
		log_error("loop_new: malloc() failed");
		free(loop->pollfds);
        free(loop);
		return NULL;
    }
    memset(loop->channels, 0, loop->max_count * sizeof(channel_t*) * 2);
	loop->active_channels = loop->channels + loop->max_count;

    loop->task_queue = async_task_queue_create(loop);
    loop->timer_queue = timer_queue_create();

    return loop;
}

void loop_destroy(loop_t *loop)
{
    if (NULL != loop)
    {
        timer_queue_destroy(loop->timer_queue);
		async_task_queue_destroy(loop->task_queue);
		
		free(loop->pollfds);
        free(loop->channels);
        free(loop);
    }

    return;
}

static int ensure_channel_slots(loop_t *loop)
{
    unsigned i;
    unsigned max_count;
	struct pollfd *pollfds;
	channel_t **channels;

    if (loop->count < loop->max_count)
    {
        return 0;
    }

    max_count = loop->max_count * 2;
	
    pollfds = (struct pollfd *)realloc(loop->pollfds, max_count * sizeof(struct pollfd));
	loop->pollfds = pollfds;
	
    channels = (channel_t**)realloc(loop->channels, max_count * sizeof(channel_t*) * 2);
	loop->channels = channels;
	loop->active_channels = loop->channels + max_count;

    for (i = loop->max_count; i < max_count; ++i)
    {
        loop->channels[i] = NULL;
    }

    loop->max_count = max_count;

    return 0;
}

int loop_update_channel(loop_t *loop, channel_t* channel)
{
    SOCKET fd;
	struct pollfd *pollfd;
    short event;
    int idx;
	
	channel_t* last_channel;

    if (NULL == loop || NULL == channel)
    {
        log_error("loop_update_channel: bad loop(%p) or bad channel(%p)", loop, channel);
        return -1;
    }

    fd = channel_getfd(channel);
    event = channel_getevent(channel);    
    idx = channel_getindex(channel);

    log_debug("loop_update_channel: fd(%lu), event(%d)", (unsigned long)fd, event);

    if (idx < 0)
    {
        if (ensure_channel_slots(loop) != 0)
		{
			log_error("loop_update_channel: ensure_channel_slots() failed, fd: %lu, event: %d", fd, event);
			return -1;
		}

        idx = loop->count;
		pollfd = &loop->pollfds[idx];
		pollfd->fd = fd;
		pollfd->events = event;
		pollfd->revents = 0;
		
		channel_setindex(channel, idx);
		loop->channels[idx] = channel;
		loop->count++;
    }
    else
    {
		assert((unsigned)idx < loop->count);

        if (0 == event)
        {
			loop->count--;
			
			/* 将尾部的那个channel移到此次被清除的channel所在的位置 */
			
			last_channel = loop->channels[loop->count];
			channel_setindex(last_channel, idx);
            loop->pollfds[idx] = loop->pollfds[loop->count];
            loop->channels[idx] = loop->channels[loop->count];
			
            channel_setindex(channel, -1);
			loop->pollfds[loop->count].fd = INVALID_SOCKET;
            loop->pollfds[loop->count].events = 0;
            loop->channels[loop->count] = NULL;
        }
        else
        {
			pollfd = &loop->pollfds[idx];
			assert(fd == pollfd->fd);
            pollfd->events = event;
        }
    }

    return 0;
}

void loop_async(loop_t* loop, void(*callback)(void *userdata), void* userdata)
{
	if (NULL == loop || NULL == callback)
	{
		log_error("loop_async: bad loop(%p) or bad callback(%p)", loop, callback);
		return;
	}

	async_task_queue_submit(loop->task_queue, callback, userdata);

    return;
}

void loop_run_inloop(loop_t* loop, void(*callback)(void *userdata), void* userdata)
{
	if (NULL == loop || NULL == callback)
	{
		return;
	}
	
	if (loop->threadId == GetCurrentThreadId())
	{
		callback(userdata);
	}
	else
	{
		async_task_queue_submit(loop->task_queue, callback, userdata);
	}

	return;
}

void loop_loop(loop_t *loop)
{
    long timeout;
    int ret;
	unsigned i;

    struct pollfd *pollfd;
    channel_t* channel;
	unsigned active_channels_count;

    if (NULL == loop)
    {
        log_error("loop_loop: bad loop");
        return;
    }
	
	loop->threadId = GetCurrentThreadId();
	loop->run = 1;

    while (loop->run != 0)
    {
		timeout = timer_queue_gettimeout(loop->timer_queue);
		ret = WSAPoll(loop->pollfds, loop->count, (int)timeout);
		if (ret > 0)
		{
			active_channels_count = 0;
			for (i = 0; i < loop->count && ret > 0; ++i)
			{
				pollfd = &loop->pollfds[i];
				if (pollfd->revents > 0)
				{
					ret--;

					channel = loop->channels[i];
					channel_setrevent(channel, pollfd->revents);
					loop->active_channels[active_channels_count] = channel;
					pollfd->revents = 0;
					active_channels_count++;
				}
			}

			for (i = 0; i < active_channels_count; ++i)
			{
				channel = loop->active_channels[i];
				channel_onevent(channel);
			}
		}
		else if (ret < 0)
		{
			log_error("loop_loop: WSAPoll() failed, errno: %d", WSAGetLastError());
		}

		timer_queue_process(loop->timer_queue);
    }

    return;
}

static void do_loop_quit(void* userdata)
{
	((loop_t*)userdata)->run = 0;

	return;
}

void loop_quit(loop_t* loop)
{
	if (NULL != loop)
	{
		if (GetCurrentThreadId() != loop->threadId)
		{
			loop_async(loop, do_loop_quit, loop);
		}
		else
		{
			loop->run = 0;
		}
	}

    return;
}

loop_timer_t* loop_runafter(loop_t* loop, unsigned interval, onexpire_f expirecb, void *userdata)
{
    unsigned long long timestamp;

    if (NULL == loop || NULL == expirecb || 0 == interval)
    {
        log_error("loop_runafter: bad loop(%p) or bad expirecb(%p) or bad interval(%u)", loop, expirecb, interval);
        return NULL;
    }

    get_current_timestamp(&timestamp);
    timestamp += interval;

    return timer_queue_add(loop->timer_queue, timestamp, 0, expirecb, userdata);
}

loop_timer_t* loop_runevery(loop_t* loop, unsigned interval, onexpire_f expirecb, void *userdata)
{
    unsigned long long timestamp;

    if (NULL == loop || NULL == expirecb || 0 == interval)
    {
        log_error("loop_runevery: bad loop(%p) or bad expirecb(%p) or bad interval(%u)", loop, expirecb, interval);
        return NULL;
    }

    get_current_timestamp(&timestamp);
    timestamp += interval;

    return timer_queue_add(loop->timer_queue, timestamp, interval, expirecb, userdata);
}

void loop_cancel(loop_t* loop, loop_timer_t *timer)
{
    if (NULL != loop && NULL != timer)
    {
        timer_queue_cancel(loop->timer_queue, timer);
    }

    return;
}

void loop_refresh(loop_t* loop, loop_timer_t *timer)
{
    if (NULL != loop && NULL != timer)
    {
        timer_queue_refresh(loop->timer_queue, timer);
    }

    return;
}

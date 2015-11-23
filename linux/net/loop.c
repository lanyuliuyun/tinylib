

#include "net/loop.h"
#include "net/timer_queue.h"
#include "net/async_task_queue.h"
#include "util/log.h"
#include "util/util.h"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <errno.h>
#include <assert.h>

#include <pthread.h>

#define MAX_EP_EVENT    (128)

struct loop
{
    unsigned run;
	pthread_t threadId;

    int epfd;
    struct epoll_event events[MAX_EP_EVENT];

	async_task_queue_t *task_queue;
    timer_queue_t *timer_queue;
};

loop_t* loop_new(unsigned hint)
{
    loop_t* loop;
    int epfd;

    loop = (loop_t*)malloc(sizeof(loop_t));
    memset(loop, 0, sizeof(*loop));
    loop->run = 0;

    epfd = epoll_create(hint);
    if (epfd < 0)
    {
        free(loop);
        log_error("loop_new: epoll_create() failed, error: %d", errno);
        return NULL;
    }

    loop->epfd = epfd;

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
		close(loop->epfd);
        free(loop);
    }
    
    return;
}

int loop_update_channel(loop_t *loop, channel_t* channel)
{
    int fd;
    int event;
    int operate;
    struct epoll_event epevent;

    if (NULL == loop || NULL == channel)
    {
        log_error("loop_update_channel: bad loop(%p) or bad channel(%p)", loop, channel);
        return -1;
    }

    event = channel_getevent(channel);
    fd = channel_getfd(channel);

    log_debug("loop_update_channel: fd(%d), event(%d)", fd, event);

    memset(&epevent, 0, sizeof(epevent));
    epevent.events = event;
    epevent.data.ptr = channel;

    if (0 == event)
    {
        operate = EPOLL_CTL_DEL;
        channel_set_monitored(channel, 0);
    }
    else
    {
        if (channel_monitored(channel))
        {
            operate = EPOLL_CTL_MOD;
        }
        else
        {
            operate = EPOLL_CTL_ADD;
            channel_set_monitored(channel, 1);
        }
    }

    if (epoll_ctl(loop->epfd, operate, fd, &epevent) < 0)
    {
        log_error("loop_update_channel: epoll_ctl() failed, operate(%d) fd(%d), errno: %d", operate, fd, errno);
		return -1;
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

	if (pthread_equal(loop->threadId, pthread_self()))
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
    int result;
    int i;
    long timeout;
    struct epoll_event *event;
    channel_t* channel;
	int error;

    if (NULL == loop)
    {
        log_error("loop_loop: bad loop");
        return;
    }
	
	loop->threadId = pthread_self();
	loop->run = 1;

    while (loop->run != 0)
    {
        timeout = timer_queue_gettimeout(loop->timer_queue);
        result = epoll_wait(loop->epfd, loop->events, MAX_EP_EVENT, timeout);
		error = errno;
        if (result > 0)
        {
            for (i = 0; i < result; ++i)
            {
                event = &(loop->events[i]);
                channel = (channel_t*)event->data.ptr;
				channel_setrevent(channel, event->events);
            }
			for (i = 0; i < result; ++i)
			{
				event = &(loop->events[i]);
                channel = (channel_t*)event->data.ptr;
				channel_onevent(channel);
			}
        }
        else if (0 > result && EINTR != error)
        {
            log_error("loop_loop: epoll_wait() failed, errno: %d", error);
        }

        timer_queue_process(loop->timer_queue);
    }

    return;
}

static void do_loop_quit(void *userdata)
{
	((loop_t*)userdata)->run = 0;

	return;
}

void loop_quit(loop_t* loop)
{
	pthread_t threadId;
	if (NULL != loop)
	{
		threadId = pthread_self();
		if (pthread_equal(loop->threadId, threadId))
		{
			loop->run = 0;
		}
		else
		{
			async_task_queue_submit(loop->task_queue, do_loop_quit, loop);
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

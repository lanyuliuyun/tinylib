
#include "util/util.h"
#include "util/log.h"
#include "net/channel.h"

#include <stdlib.h>
#include <assert.h>

struct channel
{
    SOCKET fd;
    loop_t *loop;

    on_event_f on_event;
    void* userdata;
	
	int is_active;			/* 标记当前的channel已经有IO事件待处理 */
	int is_in_callback;		/* 标记当前的channel对应的对象正在执行回调 */
	int is_alive;			/* 标记当前channel是否继续存活，专用于在执行回调被destroy时，标记自身为不存活 */

    short event;
	short revent;
    int index;
};

channel_t* channel_new(SOCKET fd, loop_t* loop, on_event_f func, void* userdata)
{
    channel_t* channel;

    if (NULL == loop || INVALID_SOCKET == fd)
    {
        log_error("channel_new: bad loop(%p) or bad fd(%lu)", loop, fd);
        return NULL;
    }

    channel = (channel_t*)malloc(sizeof(channel_t));
    memset(channel, 0, sizeof(*channel));
    channel->fd = fd;
    channel->loop = loop;
    channel->on_event = func;
    channel->userdata = userdata;
	
	channel->is_active = 0;
	channel->is_in_callback = 0;
	channel->is_alive = 1;

    channel->event = 0;
	channel->revent = 0;
    channel->index = -1;

    return channel;
}

void channel_destroy(channel_t* channel)
{
	if (NULL == channel)
	{
		return;
	}

	if (channel->is_in_callback)
	{
		channel->is_alive = 0;
	}
	else
	{
		if (channel->is_active)
		{
			channel->is_alive = 0;
		}
		else
		{
			free(channel);
		}
	}

    return;
}

short channel_getevent(channel_t* channel)
{
    return (NULL == channel) ? 0 : channel->event;
}

int channel_setevent(channel_t* channel, short event)
{
    if (NULL == channel)
    {
        log_error("channel_setevent: bad channel");
        return -1;
    }

    if ((channel->event & event) == event)
    {
        /* 该事件已经处于监测状态，无需重复添加 */
        return 0;
    }

    assert(NULL != channel->loop);
    channel->event |= event;
	
	if (loop_update_channel(channel->loop, channel) != 0)
	{
		log_error("channel_setevent(%p, %d), loop_update_channel() failed", channel, event);
		return -1;
	}

    return 0;
}

void channel_setrevent(channel_t* channel, short event)
{
	if (NULL != channel)
	{
		channel->revent = event;
		channel->is_active = 1;
	}

	return;
}

int channel_clearevent(channel_t* channel, short event)
{
    if (NULL == channel)
    {
        log_error("channel_clearevent: bad channel(%p)", channel);
        return -1;
    }

    if ((event & channel->event) == 0)
    {
        /* 所指定的事件没有在被监测，无需做实际的clear操作 */
        return 0;
    }

    assert(NULL != channel->loop);

    channel->event &= ~event;
	if (loop_update_channel(channel->loop, channel) != 0)
	{
		log_error("channel_clearevent(%p, %d) loop_update_channel() failed", channel, event);
		return -1;
	}

    return 0;
}

void channel_detach(channel_t* channel)
{
    short old_event;

    if (NULL == channel)
    {
        return;
    }
	
	log_debug("channel_detach: fd(%lu), event(%d)", channel->fd, channel->event);
	
	if (0 == channel->event)
	{
		/* 该channel未被检测，不做detach */
		return;
	}

    old_event = channel->event;
    channel->event = 0;
    if (loop_update_channel(channel->loop, channel) != 0)
	{
		log_error("channel_detach(%p): loop_update_channel() failed", channel);
	}
    channel->event = old_event;
	channel->loop = NULL;

    return;
}

void channel_attach(channel_t *channel, loop_t *loop)
{
    if (NULL == channel || NULL == loop)
    {
        log_error("channel_attach: bad channel(%p) or bad loop(%p)", channel, loop);
        return;
    }
	
	log_debug("channel_attach: fd(%lu), event(%d)", channel->fd, channel->event);

    channel->loop = loop;
    loop_update_channel(channel->loop, channel);

    return;
}

void channel_onevent(channel_t* channel)
{
    if (NULL == channel)
    {
        log_error("channel_clearevent: bad channel");
        return;
    }

    log_debug("channel_onevent: fd(%lu), event(%d)", channel->fd, channel->revent);
	
	if (0 == channel->is_alive)
	{
		channel->is_active = 0;
		channel_destroy(channel);
		return;
	}

    assert(NULL != channel->on_event);
	channel->is_in_callback = 1;
    channel->on_event(channel->fd, channel->revent, channel->userdata);
	channel->is_in_callback = 0;
	channel->revent = 0;

	/* 标记当前的IO事件已经被处理，不再处于活跃状态 */
	channel->is_active = 0;
	
	if (0 == channel->is_alive)
	{
		channel_destroy(channel);
	}

	return;
}

SOCKET channel_getfd(channel_t* channel)
{
    if (NULL == channel)
    {
        return INVALID_SOCKET;
    }

    return channel->fd;
}

int channel_getindex(channel_t* channel)
{
    return NULL == channel ? -1 : channel->index;
}

void channel_setindex(channel_t* channel, int idx)
{
    if (NULL != channel)
    {
        channel->index = idx;
    }

    return;
}


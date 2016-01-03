
#include "net/async_task_queue.h"

#include "util/lock.h"

#include <stdlib.h>		/* for NULL */
#include <string.h>		/* for memset() */
#include <unistd.h>		/* for close() */

#include <sys/eventfd.h>
#include <sys/epoll.h>

struct async_task
{
    void (*callback)(void *userdata);
    void *userdata;

    struct async_task *next;
};

struct async_task_queue
{
	loop_t *loop;
	int fd;
	channel_t *channel;

    struct async_task *async_task;
    struct async_task *async_task_end;
    lock_t task_lock;
};

static
void async_task_queue_process(async_task_queue_t *task_queue);

static
void async_task_event(int fd, int event, void* userdata)
{
	async_task_queue_t* task_queue = (async_task_queue_t*)userdata;
	eventfd_t value;

	(void)eventfd_read(task_queue->fd, &value);

	async_task_queue_process(task_queue);

	return;
}

async_task_queue_t* async_task_queue_create(loop_t *loop)
{
	async_task_queue_t* task_queue = (async_task_queue_t*)malloc(sizeof(async_task_queue_t));
	memset(task_queue, 0, sizeof(*task_queue));

	task_queue->loop = loop;

	task_queue->fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);

	task_queue->channel = channel_new(task_queue->fd, loop, async_task_event, task_queue);
	channel_setevent(task_queue->channel, EPOLLIN);

	task_queue->async_task = NULL;
	task_queue->async_task_end = NULL;
	lock_init(&task_queue->task_lock);

	return task_queue;
}

void async_task_queue_destroy(async_task_queue_t *task_queue)
{
	struct async_task *task;
	
	if (NULL == task_queue)
	{
		return;
	}

	while (NULL != task_queue->async_task)
	{
		task = task_queue->async_task;
		task_queue->async_task = task->next;
		free(task);
	}
	
	channel_detach(task_queue->channel);
	channel_destroy(task_queue->channel);
	close(task_queue->fd);

	lock_uninit(&task_queue->task_lock);

	free(task_queue);
	
	return;
}

void async_task_queue_submit(async_task_queue_t *task_queue, void(*callback)(void *userdata), void* userdata)
{
	eventfd_t value;
    struct async_task *task;

    if (NULL == task_queue || NULL == callback)
    {
        return;
    }

    task = (struct async_task*)malloc(sizeof(struct async_task));
	memset(task, 0, sizeof(*task));
	task->callback = callback;
	task->userdata = userdata;
	task->next = NULL;

	lock_it(&task_queue->task_lock);
	if (NULL == task_queue->async_task)
	{
		task_queue->async_task = task;
		task_queue->async_task_end = task;
	}
	else
	{
		task_queue->async_task_end->next = task;
		task_queue->async_task_end = task;
	}
	unlock_it(&task_queue->task_lock);

	value = 1;
	eventfd_write(task_queue->fd, value);

	return;
}

static
void async_task_queue_process(async_task_queue_t *task_queue)
{
    struct async_task *task;
    struct async_task *t_iter;

	lock_it(&task_queue->task_lock);
	task = task_queue->async_task;
	task_queue->async_task = NULL;
	task_queue->async_task_end = NULL;
	unlock_it(&task_queue->task_lock);

	while (NULL != task)
	{
		t_iter = task->next;

		task->callback(task->userdata);
		free(task);

		task = t_iter;
	}
}

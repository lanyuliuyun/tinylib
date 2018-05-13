
#include "tinylib/windows/net/async_task_queue.h"
#include "tinylib/windows/net/socket.h"
#include "tinylib/util/log.h"
#include "tinylib/util/lock.h"

#include <stdlib.h>
#include <string.h>

#include <winsock2.h>

struct async_task
{
    void (*callback)(void *userdata);
    void *userdata;

    struct async_task *next;
};

struct async_task_queue
{
    loop_t *loop;
    SOCKET fds[2];
    channel_t *channel;
    
    struct async_task *async_task;
    struct async_task *async_task_end;
    lock_t task_lock;
};

static
void async_task_queue_process(async_task_queue_t *task_queue);

static
void async_task_event(SOCKET fd, int event, void* userdata)
{
    char data[32];
    while(recv(fd, data, sizeof(data), 0) > 0);

    async_task_queue_process((async_task_queue_t*)userdata);

    return;
}

async_task_queue_t* async_task_queue_create(loop_t* loop)
{
    async_task_queue_t* task_queue = (async_task_queue_t*)malloc(sizeof(async_task_queue_t));
    memset(task_queue, 0, sizeof(*task_queue));

    task_queue->loop = loop;

    socketpair(SOCK_DGRAM, task_queue->fds);
    
    task_queue->channel = channel_new(task_queue->fds[0], loop, async_task_event, task_queue);
    channel_setevent(task_queue->channel, POLLIN);

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
    closesocket(task_queue->fds[0]);
    closesocket(task_queue->fds[1]);

    lock_uninit(&task_queue->task_lock);

    free(task_queue);
    
    return;
}

void async_task_queue_submit(async_task_queue_t *task_queue, void(*callback)(void *userdata), void* userdata)
{
    struct async_task *task;

    if (NULL == task_queue || NULL == callback)
    {
        log_error("async_task_queue_submit: bad task_queue(%p) or bad callback(%p)", task_queue, callback);
        return;
    }

    task = (struct async_task*)malloc(sizeof(*task));
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

    send(task_queue->fds[1], (const char*)&task, sizeof(task), 0);

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
    
    return;
}

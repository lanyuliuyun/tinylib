
/** 协助 loop 实现异步任务功能，非对外操作接口 */

#ifndef TINYLIB_NET_ASYNC_TASK_QUEUE_H
#define TINYLIB_NET_ASYNC_TASK_QUEUE_H

struct async_task_queue;
typedef struct async_task_queue async_task_queue_t;

#include "tinylib/windows/net/loop.h"

#ifdef __cplusplus
extern "C" {
#endif

async_task_queue_t* async_task_queue_create(loop_t *loop);

void async_task_queue_destroy(async_task_queue_t *task_queue);

void async_task_queue_submit(async_task_queue_t *task_queue, void(*callback)(void *userdata), void* userdata);

#ifdef __cplusplus
}
#endif

#endif /* !ASYNC_TASK_QUEUE_H */

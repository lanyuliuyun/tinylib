
#include "tinylib/windows/net/timer_queue.h"
#include "tinylib/util/log.h"
#include "tinylib/util/util.h"

#include <stdlib.h>
#include <string.h>

struct loop_timer
{
	unsigned long long timestamp;
	unsigned interval;
	onexpire_f expirecb;
	void *userdata;
	int is_in_callback;
	int is_alive;
	int is_expired;

	struct loop_timer *prev;
	struct loop_timer *next;
};

struct timer_queue
{
	loop_timer_t *timer_list;
	loop_timer_t *timer_list_end;
	unsigned long long min_timestamp;
};

timer_queue_t* timer_queue_create(void)
{
	timer_queue_t* timer_queue;

	timer_queue = (timer_queue_t*)malloc(sizeof(timer_queue_t));
	memset(timer_queue, 0, sizeof(*timer_queue));
	timer_queue->timer_list = NULL;
	timer_queue->timer_list_end = NULL;
	get_current_timestamp(&timer_queue->min_timestamp);

	return timer_queue;
}

void timer_queue_destroy(timer_queue_t* timer_queue)
{
	loop_timer_t *timer;

	if (NULL == timer_queue)
	{
		return;
	}

	while (NULL != timer_queue->timer_list)
	{
		timer = timer_queue->timer_list;
		timer_queue->timer_list = timer->next;
		free(timer);
	}
	free(timer_queue);

	return;
}

/* FIXME: 目前是用线性链表记录timer，timer数量多的时候，效率不会太好!  */
/* 插入结果为时间戳从小到大进行排列 */
static inline void insert_timer(timer_queue_t *timer_queue, loop_timer_t *timer)
{
	loop_timer_t *t_iter;

	assert(NULL != timer_queue && NULL != timer);

	if (NULL == timer_queue->timer_list)
	{
		timer_queue->timer_list = timer;
		timer_queue->timer_list_end = timer;

		timer_queue->min_timestamp = timer->timestamp;
	}
	else
	{
		/* 查找时间戳比给定的timer大的节点 */
		t_iter = timer_queue->timer_list;
		while (NULL != t_iter && t_iter->timestamp <= timer->timestamp)
		{
			t_iter = t_iter->next;
		}

		if (NULL == t_iter)
		{
			/* 表尾插入 */
			timer_queue->timer_list_end->next = timer;
			timer->prev = timer_queue->timer_list_end;
			timer_queue->timer_list_end = timer;
		}
		else
		{
			if (NULL == t_iter->prev)
			{
				/* 表头插入 */
				timer->prev = NULL;
				timer->next = t_iter;
				t_iter->prev = timer;
				timer_queue->timer_list = timer;
				timer_queue->min_timestamp = timer->timestamp;
			}
			else
			{
				/* 表中间插入 */
				t_iter->prev->next = timer;
				timer->prev = t_iter->prev;

				timer->next = t_iter;
				t_iter->prev = timer;
			}
		}
	}

	return;
}

static inline void remove_timer(timer_queue_t *timer_queue, loop_timer_t *timer)
{
	if (timer_queue->timer_list == timer)
	{
		/* 是表头节点，需要更新timer_queue的最小时间戳记录 */
		timer_queue->timer_list = timer->next;
		if (NULL != timer_queue->timer_list)
		{
			timer_queue->timer_list->prev = NULL;
			timer_queue->min_timestamp = timer_queue->timer_list->timestamp;
		}
		else
		{
			timer_queue->timer_list_end = NULL;
			get_current_timestamp(&timer_queue->min_timestamp);
		}
	}
	else if (timer_queue->timer_list_end == timer)
	{
		/* 是表尾节点 */
		timer_queue->timer_list_end = timer->prev;
		timer_queue->timer_list_end->next = NULL;
	}
	else
	{
		/* 在timer callback中cancel_timer时，可能timer_queue中暂时没有timer记录，会直接走到此处
		 * 因此timer->next和timer->prev均为NULL，此时需要判断一下
		 */
		if (NULL != timer->prev)
		{
			timer->prev->next = timer->next;
		}
		if (NULL != timer->next)
		{
			timer->next->prev = timer->prev;
		}
	}

	return;
}

loop_timer_t *timer_queue_add(timer_queue_t *timer_queue, unsigned long long timestamp, unsigned interval, onexpire_f expirecb, void *userdata)
{
	loop_timer_t *timer;

	if (NULL == timer_queue || NULL == expirecb)
	{
		log_error("timer_queue_add: bad timer_queue(%p) or bad expirecb(%p)");
		return NULL;
	}

	timer = (loop_timer_t*)malloc(sizeof(loop_timer_t));
	memset(timer, 0, sizeof(*timer));
	timer->timestamp = timestamp;
	timer->interval = interval;
	timer->expirecb = expirecb;
	timer->userdata = userdata;
	timer->is_in_callback = 0;
	timer->is_alive = 1;
	timer->is_expired = 0;
	timer->prev = NULL;
	timer->next = NULL;

	insert_timer(timer_queue, timer);

	return timer;
}

void timer_queue_cancel(timer_queue_t *timer_queue, loop_timer_t *timer)
{
	if (NULL == timer_queue || NULL == timer)
	{
		return;
	}

	if (timer->is_in_callback)
	{
		timer->is_alive = 0;
	}
	else
	{
		if (timer->is_expired)
		{
			timer->is_alive = 0;
		}
		else
		{
			remove_timer(timer_queue, timer);
			free(timer);
		}
	}

	return;
}

void timer_queue_refresh(timer_queue_t *timer_queue, loop_timer_t *timer)
{
	unsigned long long now;
	
	if (NULL == timer_queue || NULL == timer)
	{
		return;
	}

	if (timer->interval > 0)
	{
		remove_timer(timer_queue, timer);
		get_current_timestamp(&now);
		timer->timestamp = now + timer->interval;
		timer->prev = NULL;
		timer->next = NULL;
		insert_timer(timer_queue, timer);
	}

	return;
}

long timer_queue_gettimeout(timer_queue_t *timer_queue)
{
	unsigned long long now;
	long timeout;
	long interval;

	if (NULL == timer_queue)
	{
		return 100;
	}
	
	if (NULL == timer_queue->timer_list)
	{
		return 100;
	}

	get_current_timestamp(&now);

	interval = (long)(timer_queue->min_timestamp - now);

	timeout = 100;
	if (timeout > interval)
	{
		timeout = interval;
	}
	
	if (timeout < 0)
	{
		timeout = 0;
	}

	return timeout;
}

void timer_queue_process(timer_queue_t *timer_queue)
{
	unsigned long long now;
	loop_timer_t *timer;

	loop_timer_t *done_timer;
	loop_timer_t *done_timer_end;
	loop_timer_t *timer_iter;

	if (NULL == timer_queue)
	{
		return;
	}

	if (NULL == timer_queue->timer_list)
	{
		return;
	}

	get_current_timestamp(&now);

	/* 寻找时间戳大于now的节点，在该节点之前的所有timer均为已超时 */
	timer = timer_queue->timer_list;
	while (NULL != timer && timer->timestamp <= now)
	{
		timer = timer->next;
	}

	done_timer = NULL;
	done_timer_end = NULL;	

	if (NULL == timer)
	{
		/* 到表尾了，没有时间戳比now大的节点，表明所有的timer都超时了 */
		done_timer = timer_queue->timer_list;
		done_timer->prev = NULL;
		done_timer_end = timer_queue->timer_list_end;
		done_timer_end->next = NULL;

		timer_queue->timer_list = NULL;
		timer_queue->timer_list_end = NULL;
		timer_queue->min_timestamp = now;
	}
	else if (NULL == timer->prev)
	{
		/* 没有timer超时 */
		return;
	}
	else
	{
		/* 部分timer超时了 */
		done_timer = timer_queue->timer_list;
		done_timer_end = timer->prev;
		done_timer_end->next = NULL;

		timer_queue->timer_list = timer;
		timer_queue->timer_list->prev = NULL;
		timer_queue->min_timestamp = timer->timestamp;
	}
	
	timer_iter = done_timer;
	while (NULL != timer_iter)
	{
		timer_iter->is_expired = 1;
		timer_iter = timer_iter->next;
	}

	while (NULL != done_timer)
	{
		timer = done_timer;
		done_timer = timer->next;

		timer->prev = NULL;
		timer->next = NULL;
		if (timer->is_alive)
		{
			timer->is_in_callback = 1;
			timer->expirecb(timer->userdata);
			timer->is_in_callback = 0;
		}

		if (timer->is_alive && timer->interval != 0)
		{
			timer->timestamp += timer->interval;
			timer->is_expired = 0;
			timer->prev = NULL;
			timer->next = NULL;
			insert_timer(timer_queue, timer);
		}
		else
		{
			free(timer);
		}
	}

	return;
}

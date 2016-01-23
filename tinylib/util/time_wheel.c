
#include "time_wheel.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct wheel_bucket
{
    onexpire_f callback;
    void *userdata;
    unsigned index;
    unsigned steps;

    struct wheel_bucket *prev;
    struct wheel_bucket *next;
};

struct time_wheel
{
    unsigned max_steps;
    struct wheel_bucket **buckets;
    unsigned index;
};

time_wheel_t* time_wheel_create(unsigned max_steps)
{
    time_wheel_t *wheel;

    wheel = (time_wheel_t*)malloc(sizeof(time_wheel_t) + sizeof(struct wheel_bucket*)*max_steps);
    memset(wheel, 0, (sizeof(time_wheel_t) + sizeof(struct wheel_bucket*)*max_steps));

    wheel->max_steps = max_steps;
    wheel->buckets = (struct wheel_bucket **)((char*)wheel + sizeof(time_wheel_t));
    wheel->index = max_steps-1;    

    return wheel;
}

void time_wheel_destroy(time_wheel_t* wheel)
{
    struct wheel_bucket *bucket;
    struct wheel_bucket *b_iter;
    unsigned i;

    if (NULL == wheel)
    {
        return;
    }

    for (i = 0; i < wheel->max_steps; ++i)
    {
        bucket = wheel->buckets[i];
        while (NULL != bucket)
        {
            b_iter = bucket->next;
            free(bucket);
            bucket = b_iter;
        }
    }

    free(bucket);

	return;
}

void* time_wheel_submit(time_wheel_t* wheel, onexpire_f callback, void* userdata, unsigned steps)
{
    int index;
    struct wheel_bucket *head;
    struct wheel_bucket *bucket;

    if (NULL == wheel || NULL == callback)
    {
        log_error("time_wheel_submit: bad wheel(%p) or bad callback(%p)", wheel, callback);
        return NULL;
    }

    assert(wheel->index < wheel->max_steps);

    bucket = (struct wheel_bucket*)malloc(sizeof(struct wheel_bucket));
    memset(bucket, 0, sizeof(*bucket));
    bucket->callback = callback;
    bucket->userdata = userdata;
    bucket->prev = NULL;
    bucket->next = NULL;

    if (steps > wheel->max_steps)
    {
        index = wheel->index + 1;
        if ((unsigned)index >= wheel->max_steps)
        {
            index = 0;
        }
    }
    else
    {
        index = wheel->index - steps;
        if (index < 0)
        {
            index += wheel->max_steps;
        }
    }
    bucket->index = index;
    bucket->steps = steps;

    head = wheel->buckets[index];
    bucket->next = head;
    if (NULL != head)
    {
        head->prev = bucket;
    }
    wheel->buckets[index] = bucket;    

    return bucket;
}

void time_wheel_cancel(time_wheel_t* wheel, void *handle)
{
    struct wheel_bucket *prev;
    struct wheel_bucket *next;
    struct wheel_bucket *bucket;
    int index;

    if (NULL == wheel || NULL == handle)
    {
        return;
    }
    
    bucket = (struct wheel_bucket *)handle;
    prev = bucket->prev;
    next = bucket->next;
    index = bucket->index;

    if (NULL != prev)
    {
        prev->next = next;
    }
    if (NULL != next)
    {
        next->prev = prev;
    }

    /* 摘除的是头结点，则将之后的节点作为新的头 */
    if (wheel->buckets[index] == bucket)
    {
        wheel->buckets[index] = next;
    }

    free(bucket);

    return;
}

void time_wheel_refresh(time_wheel_t* wheel, void *handle)
{
    int index;
    struct wheel_bucket *bucket;
    struct wheel_bucket *tail;    
    struct wheel_bucket *prev;
    struct wheel_bucket *next;

    if (NULL == wheel || NULL == handle)
    {
        return;
    }

    bucket = (struct wheel_bucket *)handle;
    assert(bucket->index < wheel->max_steps);
    prev = bucket->prev;
    next = bucket->next;

    /* 将当前的的bucket节点从当前位置摘除，放入下一个超时的位置 */

    if (NULL != prev)
    {
        prev->next = next;
    }
    if (NULL != next)
    {
        next->prev = prev;
    }

    index = bucket->index;
    /* 摘除的是头结点，则将之后的节点作为新的头 */
    if (wheel->buckets[index] == bucket)
    {
        wheel->buckets[index] = next;
    }

    /* 新的位置 */
	index = wheel->index - bucket->steps;
	if (index < 0)
	{
		index += wheel->max_steps;
	}

    tail = wheel->buckets[index];
    bucket->prev = NULL;
    bucket->next = tail;
	bucket->index = index;
    if (NULL != tail)
    {
        tail->prev = bucket;
    }
    wheel->buckets[index] = bucket;

    return;
}

void time_wheel_step(time_wheel_t* wheel)
{
    struct wheel_bucket *bucket;
    struct wheel_bucket *b_iter;
    struct wheel_bucket *head;
    int index;

    if (NULL == wheel)
    {
        log_error("time_wheel_step: bad wheel");
        return;
    }

    bucket = wheel->buckets[wheel->index];

    wheel->buckets[wheel->index] = NULL;
	index = wheel->index;
	index--;
    if (index < 0)
    {
        index = wheel->max_steps - 1;
    }
	wheel->index = index;
    
    while (NULL != bucket)
    {
        b_iter = bucket->next;
        assert(NULL != bucket->callback);
        if (TIME_WHEEL_EXPIRE_ONESHOT == bucket->callback(bucket->userdata))
        {
            free(bucket);
        }
        else /* if (TIME_WHEEL_EXPIRE_LOOP == bucket->callback(bucket->userdata))  */
        {
            /* 进入下一个周期的计时 */
        	index = wheel->index - bucket->steps + 1;
        	if (index < 0)
    		{
    			index += wheel->max_steps;
    		}

            head = wheel->buckets[index];
            bucket->next = head;
            bucket->prev = NULL;
            if (NULL != head)
            {
                head->prev = bucket;
            }
            wheel->buckets[index] = bucket;
            bucket->index = index;
        }

        bucket = b_iter;
    }

    return;
}

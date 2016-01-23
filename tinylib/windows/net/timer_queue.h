
#ifndef NET_TIMER_QUEUE_H
#define NET_TIMER_QUEUE_H

struct timer_queue;
typedef struct timer_queue timer_queue_t;

#include "tinylib/windows/net/timer.h"

#ifdef __cplusplus
extern "C" {
#endif

timer_queue_t* timer_queue_create(void);

void timer_queue_destroy(timer_queue_t* timer_queue);

/* 新增一个timer，在指定时刻执行用户函数, 若interval非0，则表示是一个周期性timer */
loop_timer_t *timer_queue_add(timer_queue_t *timer_queue, unsigned long long timestamp, unsigned interval, onexpire_f expirecb, void *userdata);

void timer_queue_cancel(timer_queue_t *timer_queue, loop_timer_t *timer);
/* 仅对interval值非0的timer有效 */
void timer_queue_refresh(timer_queue_t *timer_queue, loop_timer_t *timer);

/* 获取从此刻举例往后最近的一个timer的时差 */
long timer_queue_gettimeout(timer_queue_t *timer_queue);

void timer_queue_process(timer_queue_t *timer_queue);

#ifdef __cplusplus
}
#endif

#endif /* !NET_TIMER_QUEUE_H */


#ifndef NET_LOOP_H
#define NET_LOOP_H

struct loop;
typedef struct loop loop_t;

#include "net/timer.h"
#include "net/channel.h"

#ifdef __cplusplus
extern "C" {
#endif

loop_t* loop_new(unsigned hint);

void loop_destroy(loop_t *loop);

int loop_update_channel(loop_t *loop, channel_t* channel);

/* 提交一个异步执行任务，该方法是线程安全的 */
/* 请不要频繁的提交异步任务，否则会使得内核态执行时间明显上升 */
void loop_async(loop_t* loop, void(*callback)(void *userdata), void* userdata);

/* 在指定循环中执行一个回调方法，该方法是线程安全的
 * 请不要频繁地跨线程使用该接口，否则会使得内核态执行时间明显上升
 */
void loop_run_inloop(loop_t* loop, void(*callback)(void *userdata), void* userdata);

void loop_loop(loop_t* loop);

void loop_quit(loop_t* loop);

/* 启动定时器，以ms为单位 */
/* 一次性timer，超时之后会被自动回收，不需手动cancel */
loop_timer_t* loop_runafter(loop_t* loop, unsigned interval, onexpire_f expirecb, void *userdata);
/* 周期性timer */
loop_timer_t* loop_runevery(loop_t* loop, unsigned interval, onexpire_f expirecb, void *userdata);

void loop_cancel(loop_t* loop, loop_timer_t *timer);
/* 仅对loop_runevery返回的循环timer有效 */
void loop_refresh(loop_t* loop, loop_timer_t *timer);

#ifdef __cplusplus
}
#endif

#endif /* NET_LOOP_H */


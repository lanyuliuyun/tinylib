
/** 本 eventloop 对外接口功能，接口方法均是线程安全的 */

#ifndef NET_LOOP_H
#define NET_LOOP_H

struct loop;
typedef struct loop loop_t;

#include "tinylib/linux/net/timer.h"
#include "tinylib/linux/net/channel.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 新建一个事件循环
 */
loop_t* loop_new(unsigned hint);

/* 销毁给定的事件循环
 * 要求必须在事件循环结束(loop_loop()返回)之后进行
 */
void loop_destroy(loop_t *loop);

/* 更新指定 channel 的IO事件检测过程
 * 若给定的 channel 不含任何待检测事件，对应的检测过程将停止
 */
int loop_update_channel(loop_t *loop, channel_t* channel);

/* 提交一个异步执行任务，调用立即返回
 * 请不要频繁的提交异步任务，否则会使得内核态执行时间明显上升 
 */
void loop_async(loop_t* loop, void(*callback)(void *userdata), void* userdata);

/* 在指定循环中执行一个回调方法，如果已经在 loop 运行的线程里，则直接执行提供的回调
 * 请不要频繁地跨线程使用该接口，否则会使得内核态执行时间明显上升
 */
void loop_run_inloop(loop_t* loop, void(*callback)(void *userdata), void* userdata);

/* 检查调用线程是否与 loop 线程是同一个线程
 */
int loop_inloopthread(loop_t* loop);

/* 启动事件循环，该方法持续运行，直至 loop_quit() 被调用
 */
void loop_loop(loop_t* loop);

/* 结束 loop_loop() 启动的事件循环
 */
void loop_quit(loop_t* loop);

/* 启动一次性timer定时器，以ms为单位 
 * 超时之后会被自动回收，不需手动cancel 
 */
loop_timer_t* loop_runafter(loop_t* loop, unsigned interval, onexpire_f expirecb, void *userdata);

/* 启动周期性timer 
 */
loop_timer_t* loop_runevery(loop_t* loop, unsigned interval, onexpire_f expirecb, void *userdata);

/* 取消周期性timer或未超时的一次性timer
 */
void loop_cancel(loop_t* loop, loop_timer_t *timer);

/* 刷新周期性timer
 */
void loop_refresh(loop_t* loop, loop_timer_t *timer);

#ifdef __cplusplus
}
#endif

#endif /* NET_LOOP_H */


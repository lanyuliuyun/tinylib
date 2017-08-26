
/** 协助 loop 实现IO事件检测功能，每个待检测的fd，请对应分配一个channel */

#ifndef TINYLIB_NET_CHANNEL_H
#define TINYLIB_NET_CHANNEL_H

struct channel;
typedef struct channel channel_t;

#include "tinylib/linux/net/loop.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*on_event_f)(int fd, int event, void* userdata);

channel_t* channel_new(int fd, loop_t* loop, on_event_f callback, void* userdata);

void channel_destroy(channel_t* channel);

int channel_getevent(channel_t* channel);

int channel_setevent(channel_t* channel, int event);

void channel_setrevent(channel_t* channel, int event);

int channel_clearevent(channel_t* channel, int event);

void channel_detach(channel_t* channel);

void channel_attach(channel_t *channel, loop_t *loop);

void channel_onevent(channel_t* channel);

int channel_getfd(channel_t* channel);

loop_t* channel_getloop(channel_t* channel);

int channel_monitored(channel_t* channel);

void channel_set_monitored(channel_t* channel, int on);

#ifdef __cplusplus
}
#endif

#endif /* !TINYLIB_NET_CHANNEL_H */



#ifndef NET_CHANNEL_H
#define NET_CHANNEL_H

struct channel;
typedef struct channel channel_t;

#include "tinylib/windows/net/loop.h"

#include <winsock2.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*on_event_f)(SOCKET fd, short event, void* userdata);

channel_t* channel_new(SOCKET fd, loop_t* loop, on_event_f func, void* userdata);

void channel_destroy(channel_t* channel);

short channel_getevent(channel_t* channel);

int channel_setevent(channel_t* channel, short event);

void channel_setrevent(channel_t* channel, short event);

int channel_clearevent(channel_t* channel, short event);

void channel_detach(channel_t* channel);

void channel_attach(channel_t *channel, loop_t *loop);

void channel_onevent(channel_t* channel);

SOCKET channel_getfd(channel_t* channel);

int channel_getindex(channel_t* channel);

void channel_setindex(channel_t* channel, int idx); 

#ifdef __cplusplus
}
#endif

#endif /* !NET_CHANNEL_H */

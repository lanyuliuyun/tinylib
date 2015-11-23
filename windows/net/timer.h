
#ifndef NET_TIMER_H
#define NET_TIMER_H

#ifdef __cplusplus
extern "C" {
#endif

struct loop_timer;
typedef struct loop_timer loop_timer_t;

typedef void (*onexpire_f)(void* userdata);

#ifdef __cplusplus
}
#endif

#endif /* !NET_TIMER_H */

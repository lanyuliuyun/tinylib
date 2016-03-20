
#ifdef WINNT
    #include "tinylib/windows/net/loop.h"
#elif defined(__linux__)
    #include "tinylib/linux/net/loop.h"
#endif

#include "tinylib/util/log.h"

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static loop_t *g_loop = NULL;

static 
void onexpire1(void* userdata)
{
    log_info("onexpire1");

    return;
}

static 
void onexpire2(void* userdata)
{
    log_info("onexpire2");

    return;
}

int main(int argc, char const *argv[])
{
    loop_timer_t *timer1;
    loop_timer_t *timer2;

    g_loop = loop_new(1);
    assert(g_loop);

    timer1 = loop_runevery(g_loop, 67, onexpire1, &timer1);
    timer2 = loop_runevery(g_loop, 20, onexpire2, &timer2);

    loop_loop(g_loop);

    loop_destroy(g_loop);
    
    return 0;
}

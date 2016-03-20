
#ifdef WINNT
    #include "tinylib/windows/net/loop.h"
    #include <winsock2.h>
#elif defined(__linux__)
    #include "tinylib/linux/net/loop.h"
#endif

#include "tinylib/util/log.h"

#include <stdlib.h>

static
loop_t *loop = NULL;

static
void quit(void* userdata)
{
    log_info("quit");
    loop_quit(loop);
    
    return;
}

static
void on_timer(void* userdata)
{
    static int counter = 3;
    
    log_info("timer");

    if (counter == 0)
    {
        loop_async(loop, quit, NULL);
    }
    counter--;

    return;
}

int main(int argc, char *argv[])
{
    loop_timer_t *timer;
    
    loop = loop_new(64);
    timer = loop_runevery(loop, 1000, on_timer, NULL);
    
    loop_loop(loop);
    
    loop_cancel(loop, timer);
    loop_destroy(loop);

    return 0;
}


/* 测试多线程场景中，异步任务的功能
 */
 
#ifdef WIN32
    #include "tinylib/windows/net/loop.h"
    #include <winsock2.h>
#elif defined(__linux__)
    #include "tinylib/linux/net/loop.h"
#endif

#include "tinylib/util/log.h"

#include <pthread.h>
#include <unistd.h>
#include <time.h>

#include <stdlib.h>

static
loop_t *loop = NULL;

static
void user_routine(void* userdata)
{
    log_info("user_routine");

    return;
}

static
void* thread_entry(void* arg)
{
    while (1)
    {
        log_info("sub thread call loop_runafter()");
        loop_runafter(loop, 1000, user_routine, NULL);
        usleep((random() % 1000) + 1594);
    }
    
    return (void*)0;
}

int main(int argc, char *argv[])
{
    pthread_t th1, th2;
    
    srandom((unsigned)time(NULL));
    
    loop = loop_new(64);
    
    pthread_create(&th1, NULL, thread_entry, NULL);
    pthread_create(&th2, NULL, thread_entry, NULL);
    
    loop_loop(loop);

    loop_destroy(loop);

    return 0;
}
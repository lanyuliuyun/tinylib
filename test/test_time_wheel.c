
#include <time.h>
#include <stdio.h>
#include <assert.h>

#if defined(WIN32)
    #include <windows.h>
    #define usleep Sleep
#elif defined(__linux__)
    #include <unistd.h>
#endif

#include "tinylib/util/time_wheel.h"

static int g_count1 = 10;
static 
int onexpire1(void *userdata)
{
    time_t t;
    struct tm *tm;
    
    t = time(NULL);
    tm = gmtime(&t);

    g_count1--;    
    printf("1 count: %d, time: %s", g_count1, asctime(tm));

    return (g_count1 > 0) ? TIME_WHEEL_EXPIRE_LOOP : TIME_WHEEL_EXPIRE_ONESHOT;
}

static int g_run = 1;
int main()
{
    time_wheel_t* wheel;
    void *timer;
    unsigned count = 0;
    
    wheel = time_wheel_create(10);
    assert(NULL != wheel);
    timer = time_wheel_submit(wheel, onexpire1, NULL, 3);

    while (g_run)
    {
        time_wheel_step(wheel);
        /* 驱动timer周期为1s */
        usleep(1000000);
        
        count++;
        
        if (count == 5)
        {
            time_wheel_refresh(wheel, timer);
        }
    }
    
    return 0;
}

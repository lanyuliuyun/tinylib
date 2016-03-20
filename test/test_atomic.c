
#include "tinylib/util/atomic.h"

#include <stdio.h>
#include <string.h>
#include <pthread.h>

atomic_t counter1 = 0;
unsigned long counter2 = 0;

static void* worker1(void *arg)
{
    unsigned count = 3000;
    while (count > 0)
    {
        (void)atomic_inc(&counter1);
        counter2++;
        count--;
    }
    
    return (void*)0;
}

static void* worker2(void *arg)
{
    unsigned count = 3000;
    while (count > 0)
    {
        (void)atomic_dec(&counter1);
        counter2--;
        count--;
    }
    
    return (void*)0;
}

int main(int argc, char const *argv[])
{
    pthread_t th[2];
    memset(th, 0, sizeof(th));

    printf("before g_counter1: %d, g_counter2: %d\n", (int)counter1, (int)counter2);

    pthread_create(&th[0], NULL, worker1, NULL);
    pthread_create(&th[1], NULL, worker2, NULL);
    
    pthread_join(th[0], NULL);
    pthread_join(th[1], NULL);
    
    printf("after g_counter1: %d, g_counter2: %d\n", (int)counter1, (int)counter2);
    
    return 0;
}
